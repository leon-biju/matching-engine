# Command and event interfaces

This file defines the commands the engine receives and the events it produces. Field types and matching rules are defined in [Matching semantics](semantics.md).

## Commands

* **NewOrder:** Order ID, symbol, side, order type, quantity, and limit price when applicable.
* **CancelOrder:** Target order ID.

## Events

Common fields on every event:

* Engine sequence
* Event index
* Symbol, when the command can be associated with a valid symbol

Event-specific fields:

* **OrderAccepted:** Order ID, side, order type, original quantity, and limit price when applicable.
* **OrderRejected:** Order ID and a stable rejection reason code.
* **Trade:** Maker order ID, taker order ID, execution price, executed quantity, and aggressor side. The resting order is the maker and the incoming order is the taker.
* **OrderRested:** Order ID, price, and remaining quantity.
* **OrderCancelled:** Order ID, cancelled remaining quantity, and reason (`UserRequested` or `IOCExpired`).
* **CancelRejected:** Target order ID and a stable rejection reason code.

## Event ordering

Each input command produces exactly one complete event batch. Batches are published in global engine-sequence order, with no interleaving between commands. Events within a batch have a zero-based event index. The pair `(engine sequence, event index)` gives every output event a stable global order. A slower command may delay publication of later batches, but cannot change their order.

* A rejected new order emits only `OrderRejected`.
* An accepted new order emits `OrderAccepted`, followed by zero or more `Trade` events in execution order.
* If an accepted GTC order has remaining quantity, `OrderRested` is the final event.
* If an accepted IOC order has remaining quantity, `OrderCancelled` with reason `IOCExpired` is the final event. A fully filled IOC has no cancellation event.
* A successful cancel command emits one `OrderCancelled` with reason `UserRequested`. A failed cancel emits only `CancelRejected`.

## Rejection reasons

New-order rejection reasons are `UnknownSymbol`, `DuplicateOrderId`, `InvalidQuantity`, `InvalidPrice`, `InvalidSide`, `InvalidOrderType`, and `InsufficientLiquidity`. Cancel rejection reasons are `UnknownOrder` and `OrderNotActive`.
