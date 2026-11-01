# Interfaces
As described in the [Architecture](architecture.md), commands are passed into the engine, and events out of it, through SPSC queues.

The engine reads `EngineCommand` values from the command source queue. For each command it sends one batch holding every event that the command caused to the event sink. For example, one market buy order can produce several trades at different price levels before it is filled.

So one `EngineCommand` into the engine produces exactly one `EventBatch` out.

## Engine command
There are two types of engine command:
* **NewOrder:** order ID, symbol ID, side, order type, quantity, and an optional price (required for limit orders, absent for market orders).
* **CancelOrder:** order ID.

## Events
| Event | Fields | Meaning |
| --- | --- | --- |
| `OrderAccepted` | order ID | The order passed validation. |
| `OrderRejected` | order ID, reason | The order failed validation and never touched the book. |
| `Trade` | aggressor order ID, resting order ID, aggressor side, price, quantity | Part or all of the incoming order matched one resting order. |
| `OrderRested` | order ID, price, remaining quantity | The remainder of an order was added to the book. |
| `OrderCancelled` | order ID, remaining quantity | The quantity that was removed from the book, or that was left unfilled by an IOC or FOK order. |
| `CancelRejected` | order ID, reason | A cancel named an order that is not resting. |

The reject reasons are `DuplicateOrderId`, `UnknownSymbol`, `InvalidQuantity`, `InvalidPrice` and `NoLiquidity` for new orders, and `UnknownOrder` for cancels. See [Semantics](semantics.md#validation) for when each applies.

## Event order within a batch
The events in a batch always follow one of these patterns depending on the command issued to the engine.

**NewOrder**
* Failed validation: `OrderRejected`.
* Accepted: `OrderAccepted`, then zero or more `Trade`s in the order they happened (price-time priority), then at most one final event:
  * LimitGTC with a remainder: `OrderRested`.
  * IOC with a remainder: `OrderCancelled`.
  * FOK that could not be filled completely: `OrderCancelled` for the full quantity, with zero trades before it.
  * Fully filled: no final event. The trades account for the whole quantity.

**CancelOrder**
* The order was resting: `OrderCancelled`.
* Otherwise: `CancelRejected`.

## Event batch
Each batch is stamped with the engine sequence of the command that caused it, so every batch has a unique sequence number.

Batches are published in engine-sequence order, and the events of different commands are never interleaved.

An engine sends a batch to any type that satisfies the `matching::EventSink` concept in [event_sink.hpp](../include/matching/core/event_sink.hpp). For each command it calls `begin_batch` once with the command's sequence, then `add` once for each event in order, then `end_batch`. The sink decides what to do with them. `BatchCollector` collects them into `EventBatch` values, and `QueueSink` writes them straight into an SPSC queue (see [Pipeline](architecture.md#pipeline)).
