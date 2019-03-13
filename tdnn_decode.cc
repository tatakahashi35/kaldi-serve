#include <Python.h>

#include "feat/wave-reader.h"
#include "online2/online-nnet3-decoding.h"
#include "online2/online-nnet2-feature-pipeline.h"
#include "online2/onlinebin-util.h"
#include "online2/online-endpoint.h"
#include "fstext/fstext-lib.h"
#include "lat/lattice-functions.h"
#include "util/kaldi-thread.h"
#include "nnet3/nnet-utils.h"

#define VERBOSE 1

namespace kaldi {
  void GetDiagnosticsAndPrintOutput(const std::string &utt,
                                    const fst::SymbolTable *word_syms,
                                    const CompactLattice &clat,
                                    int64 *tot_num_frames,
                                    double *tot_like) {
    if (clat.NumStates() == 0) {
      std::cout << "Empty lattice.";
      return;
    }
    CompactLattice best_path_clat;
    CompactLatticeShortestPath(clat, &best_path_clat);

    Lattice best_path_lat;
    ConvertLattice(best_path_clat, &best_path_lat);

    double likelihood;
    LatticeWeight weight;
    int32 num_frames;
    std::vector<int32> alignment;
    std::vector<int32> words;
    GetLinearSymbolSequence(best_path_lat, &alignment, &words, &weight);
    num_frames = alignment.size();
    likelihood = -(weight.Value1() + weight.Value2());
    *tot_num_frames += num_frames;
    *tot_like += likelihood;
    std::cout << "Likelihood per frame for utterance " << utt << " is "
              << (likelihood / num_frames) << " over " << num_frames << " frames.";

    if (word_syms != NULL) {
      std::cerr << utt << ' ';
      for (size_t i = 0; i < words.size(); i++) {
        std::string s = word_syms->Find(words[i]);
        if (s == "") {
          std::cout << "Word-id " << words[i] << " not in symbol table.";
        }
        std::cerr << s << ' ';
      }
      std::cerr << std::endl;
    }
  }

  int CLoadModel(BaseFloat beam,
                int32 max_active,
                int32 min_active,
                BaseFloat lattice_beam,
                BaseFloat acoustic_scale,
                int32 frame_subsampling_factor,
                char* &word_syms_filename,
                char* &model_in_filename,
                char* &fst_in_str,
                char* &mfcc_config,
                char* &ie_conf_filename) {
    try {
      using namespace fst;

      typedef int32 int32;
      typedef int64 int64;

      #if VERBOSE
        KALDI_LOG << "model_in_filename:         " << model_in_filename;
        KALDI_LOG << "fst_in_str:                " << fst_in_str;
        KALDI_LOG << "mfcc_config:               " << mfcc_config;
        KALDI_LOG << "ie_conf_filename:          " << ie_conf_filename;
      #endif

      // feature_opts includes configuration for the iVector adaptation,
      // as well as the basic features.
      OnlineNnet2FeaturePipelineConfig feature_opts;
      nnet3::NnetSimpleLoopedComputationOptions decodable_opts;
      LatticeFasterDecoderConfig decoder_opts;
      // OnlineEndpointConfig endpoint_opts;

      feature_opts.mfcc_config                   = mfcc_config;
      feature_opts.ivector_extraction_config     = ie_conf_filename;
      decoder_opts.max_active                    = max_active;
      decoder_opts.min_active                    = min_active;
      decoder_opts.beam                          = beam;
      decoder_opts.lattice_beam                  = lattice_beam;
      decodable_opts.acoustic_scale              = acoustic_scale;
      decodable_opts.frame_subsampling_factor    = frame_subsampling_factor;

      BaseFloat chunk_length_secs = 0.18;

      OnlineNnet2FeaturePipelineInfo feature_info(feature_opts);

      TransitionModel trans_model;
      nnet3::AmNnetSimple am_nnet;
      {
        bool binary;
        Input ki(model_in_filename, &binary);
        trans_model.Read(ki.Stream(), binary);
        am_nnet.Read(ki.Stream(), binary);
        SetBatchnormTestMode(true, &(am_nnet.GetNnet()));
        SetDropoutTestMode(true, &(am_nnet.GetNnet()));
        nnet3::CollapseModel(nnet3::CollapseModelConfig(), &(am_nnet.GetNnet()));
      }

      // this object contains precomputed stuff that is used by all decodable
      // objects.  It takes a pointer to am_nnet because if it has iVectors it has
      // to modify the nnet to accept iVectors at intervals.
      nnet3::DecodableNnetSimpleLoopedInfo decodable_info(decodable_opts, &am_nnet);

      fst::Fst<fst::StdArc> *decode_fst = ReadFstKaldiGeneric(fst_in_str);

      fst::SymbolTable *word_syms = NULL;
      if (word_syms_filename != "" && !(word_syms = fst::SymbolTable::ReadText(word_syms_filename)))
        KALDI_ERR << "Could not read symbol table from file " << word_syms_filename;

      int32 num_done = 0, num_err = 0;
      double tot_like = 0.0;
      int64 num_frames = 0;

      SequentialTokenVectorReader spk2utt_reader("ark:echo utterance-id1 utterance-id1|");
      RandomAccessTableReader<WaveHolder> wav_reader("scp:echo utterance-id1 /home/app/1545203318.9486.wav|");
      // CompactLatticeWriter clat_writer(clat_wspecifier);

      for (; !spk2utt_reader.Done(); spk2utt_reader.Next()) {
        std::string spk = spk2utt_reader.Key();
        const std::vector<std::string> &uttlist = spk2utt_reader.Value();
        OnlineIvectorExtractorAdaptationState adaptation_state(feature_info.ivector_extractor_info);

        for (size_t i = 0; i < uttlist.size(); i++) {
          std::string utt = uttlist[i];
          if (!wav_reader.HasKey(utt)) {
            KALDI_WARN << "Did not find audio for utterance " << utt;
            num_err++;
            continue;
          }
          const WaveData &wave_data = wav_reader.Value(utt);
          // get the data for channel zero (if the signal is not mono, we only
          // take the first channel).
          SubVector<BaseFloat> data(wave_data.Data(), 0);

          OnlineNnet2FeaturePipeline feature_pipeline(feature_info);
          feature_pipeline.SetAdaptationState(adaptation_state);

          OnlineSilenceWeighting silence_weighting(
            trans_model,
            feature_info.silence_weighting_config,
            decodable_opts.frame_subsampling_factor
          );

          SingleUtteranceNnet3Decoder decoder(
            decoder_opts, trans_model, decodable_info, *decode_fst, &feature_pipeline
          );
          // OnlineTimer decoding_timer(utt);

          BaseFloat samp_freq = wave_data.SampFreq();
          int32 chunk_length;
          if (chunk_length_secs > 0) {
            chunk_length = int32(samp_freq * chunk_length_secs);
            if (chunk_length == 0) chunk_length = 1;
          } else {
            chunk_length = std::numeric_limits<int32>::max();
          }

          int32 samp_offset = 0;
          std::vector<std::pair<int32, BaseFloat> > delta_weights;

          while (samp_offset < data.Dim()) {
            int32 samp_remaining = data.Dim() - samp_offset;
            int32 num_samp = chunk_length < samp_remaining ? chunk_length : samp_remaining;

            SubVector<BaseFloat> wave_part(data, samp_offset, num_samp);
            feature_pipeline.AcceptWaveform(samp_freq, wave_part);

            samp_offset += num_samp;
            // decoding_timer.WaitUntil(samp_offset / samp_freq);
            if (samp_offset == data.Dim()) {
              // no more input. flush out last frames
              feature_pipeline.InputFinished();
            }

            if (silence_weighting.Active() && feature_pipeline.IvectorFeature() != NULL) {
              silence_weighting.ComputeCurrentTraceback(decoder.Decoder());
              silence_weighting.GetDeltaWeights(feature_pipeline.NumFramesReady(),
                                                &delta_weights);
              feature_pipeline.IvectorFeature()->UpdateFrameWeights(delta_weights);
            }

            decoder.AdvanceDecoding();
          }
          decoder.FinalizeDecoding();

          CompactLattice clat;
          bool end_of_utterance = true;
          decoder.GetLattice(end_of_utterance, &clat);

          GetDiagnosticsAndPrintOutput(utt, word_syms, clat, &num_frames, &tot_like);

          // In an application you might avoid updating the adaptation state if
          // you felt the utterance had low confidence.  See lat/confidence.h
          feature_pipeline.GetAdaptationState(&adaptation_state);

          // we want to output the lattice with un-scaled acoustics.
          BaseFloat inv_acoustic_scale = 1.0 / decodable_opts.acoustic_scale;
          ScaleLattice(AcousticLatticeScale(inv_acoustic_scale), &clat);

          // clat_writer.Write(utt, clat);
          KALDI_LOG << "Decoded utterance " << utt;
          num_done++;
        }
      }

      KALDI_LOG << "Decoded " << num_done << " utterances, " << num_err << " with errors.";
      KALDI_LOG << "Overall likelihood per frame was " << (tot_like / num_frames) << " per frame over " << num_frames << " frames.";
      delete decode_fst;
      delete word_syms; // will delete if non-NULL.

      return (num_done != 0 ? 1 : 0);
    }
    catch (const std::exception &e) {
      std::cout << e.what();
      return -1; // model not loaded
    }
  }

  char* CInfer(int n, int k) {
    return "hello";
  }
}

static PyObject *load_model(PyObject *self, PyObject *args) {
  kaldi::BaseFloat beam;
  int32 max_active;
  int32 min_active;
  kaldi::BaseFloat lattice_beam;
  kaldi::BaseFloat acoustic_scale;
  int32 frame_subsampling_factor;
  char* word_syms_filename;
  char* model_in_filename;
  char* fst_in_str;
  char* mfcc_config;
  char* ie_conf_filename;

  if (!PyArg_ParseTuple(
      args,
      "fiiffisssss",
      &beam, &max_active, &min_active, &lattice_beam,&acoustic_scale, &frame_subsampling_factor,
      &word_syms_filename, &model_in_filename, &fst_in_str, &mfcc_config, &ie_conf_filename
    )
  ) return NULL;

  return Py_BuildValue("i", kaldi::CLoadModel(beam, max_active, min_active, lattice_beam,
    acoustic_scale, frame_subsampling_factor, word_syms_filename, model_in_filename,
    fst_in_str, mfcc_config, ie_conf_filename)
  );
}

static PyObject *infer(PyObject *self, PyObject *args) {
  int n, k;

  if (!PyArg_ParseTuple(args, "ii", &n, &k)) return NULL;
  return Py_BuildValue("s", kaldi::CInfer(n, k));
}

// Our Module's Function Definition struct
// We require this `NULL` to signal the end of our method definition
static PyMethodDef moduleMethods[] = {
  {"load_model", load_model, METH_VARARGS, "Loads TDNN Model"},
  {"infer", infer, METH_VARARGS, "Converts audio to text"},
  {NULL, NULL, 0, NULL}
};

// Our Module Definition struct
static struct PyModuleDef tdnnDecode = {
  PyModuleDef_HEAD_INIT,
  "tdnn_decode",
  "Kaldi bindings for online TDNN decode",
  -1,
  moduleMethods
};

// Initializes our module using our above struct
PyMODINIT_FUNC PyInit_tdnn_decode(void) {
  return PyModule_Create(&tdnnDecode);
}