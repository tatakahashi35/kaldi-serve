# Generated by the gRPC Python protocol compiler plugin. DO NOT EDIT!
import grpc

import kaldi_serve.kaldi_serve_pb2 as kaldi__serve__pb2


class KaldiServeStub(object):
  # missing associated documentation comment in .proto file
  pass

  def __init__(self, channel):
    """Constructor.

    Args:
      channel: A grpc.Channel.
    """
    self.Recognize = channel.unary_unary(
        '/kaldi_serve.KaldiServe/Recognize',
        request_serializer=kaldi__serve__pb2.RecognizeRequest.SerializeToString,
        response_deserializer=kaldi__serve__pb2.RecognizeResponse.FromString,
        )
    self.StreamingRecognize = channel.stream_unary(
        '/kaldi_serve.KaldiServe/StreamingRecognize',
        request_serializer=kaldi__serve__pb2.RecognizeRequest.SerializeToString,
        response_deserializer=kaldi__serve__pb2.RecognizeResponse.FromString,
        )
    self.BidiStreamingRecognize = channel.stream_stream(
        '/kaldi_serve.KaldiServe/BidiStreamingRecognize',
        request_serializer=kaldi__serve__pb2.RecognizeRequest.SerializeToString,
        response_deserializer=kaldi__serve__pb2.RecognizeResponse.FromString,
        )


class KaldiServeServicer(object):
  # missing associated documentation comment in .proto file
  pass

  def Recognize(self, request, context):
    """Performs synchronous non-streaming speech recognition.
    """
    context.set_code(grpc.StatusCode.UNIMPLEMENTED)
    context.set_details('Method not implemented!')
    raise NotImplementedError('Method not implemented!')

  def StreamingRecognize(self, request_iterator, context):
    """Performs synchronous client-to-server streaming speech recognition: 
    receive results after all audio has been streamed and processed.
    """
    context.set_code(grpc.StatusCode.UNIMPLEMENTED)
    context.set_details('Method not implemented!')
    raise NotImplementedError('Method not implemented!')

  def BidiStreamingRecognize(self, request_iterator, context):
    """Performs synchronous bidirectional streaming speech recognition: 
    receive results as the audio is being streamed and processed.
    """
    context.set_code(grpc.StatusCode.UNIMPLEMENTED)
    context.set_details('Method not implemented!')
    raise NotImplementedError('Method not implemented!')


def add_KaldiServeServicer_to_server(servicer, server):
  rpc_method_handlers = {
      'Recognize': grpc.unary_unary_rpc_method_handler(
          servicer.Recognize,
          request_deserializer=kaldi__serve__pb2.RecognizeRequest.FromString,
          response_serializer=kaldi__serve__pb2.RecognizeResponse.SerializeToString,
      ),
      'StreamingRecognize': grpc.stream_unary_rpc_method_handler(
          servicer.StreamingRecognize,
          request_deserializer=kaldi__serve__pb2.RecognizeRequest.FromString,
          response_serializer=kaldi__serve__pb2.RecognizeResponse.SerializeToString,
      ),
      'BidiStreamingRecognize': grpc.stream_stream_rpc_method_handler(
          servicer.BidiStreamingRecognize,
          request_deserializer=kaldi__serve__pb2.RecognizeRequest.FromString,
          response_serializer=kaldi__serve__pb2.RecognizeResponse.SerializeToString,
      ),
  }
  generic_handler = grpc.method_handlers_generic_handler(
      'kaldi_serve.KaldiServe', rpc_method_handlers)
  server.add_generic_rpc_handlers((generic_handler,))