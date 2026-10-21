# Project Architecture
For both versions the planned command flow is having a arbitrary `Command Source` that feeds in commands to the `Matching Engine` and produces outputs to an `Event Sink`.

These sources and sinks can range from file readers to network streams.
An SPSC queue will be used for feeding data to/from source and sink.
