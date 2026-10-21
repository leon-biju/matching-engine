# Interface
As described in the [Architecture](architecture.md), the main way of passing (?) to/from the engine will be through a SPSC queue.

For the inputs to the engine from the Command Source we will use a `EngineCommand` struct.

For the outputs from the engine we will use an `EventBatch` struct comprising of a list of all the events that the command caused.

For example: 1 market BUY Order received by the engine could produce multiple trades at multiple different price levels until the BUY order is filled.

So the model is one `EngineCommand` into the Engine will pop out exactly one `EventBatch`.

## Engine Command
Two types of Engine Commands and their fields:
* **NewOrder:** Order ID, symbol, side, order type, quantity, price.
* **CancelOrder:** Order ID.

## Event
These are the types of event:
(TODO: Mark the fields in here that make sense)
*  **OrderAccepted**
*  **OrderRejected**
*  **Trade**
*  **OrderRested**
*  **OrderCancelled**
*  **CancelRejected**


Events such as OrderAccepted are followed by zero or more `Trade` events.

## Event Batch
Now, each list of Events that is caused by an incoming order will be stamped by the Engine with a sequence number.

Batches are published in global engine-sequence order, with no interleaving between command.