# Matching semantics

This document defines the order model and matching rules for both engines. Commands and output events are defined in [Command and event interfaces](interfaces.md). For now we support limit and market orders. Amend/modify commands are not supported, so the user must cancel then open a new fresh order.

## Order Model

* **Order ID:** An unsigned 64-bit integer. IDs are global across all symbols. Once an order has been accepted, its ID cannot be reused during the lifetime of that engine instance, including after the order is filled or cancelled. An ID from a rejected order may be submitted again.
* **Symbol:** A pre-configured instrument identifier. Symbols are matched exactly and are case-sensitive.
* **Side:** Buy or Sell.
* **Order type:** LimitGTC, LimitIOC, LimitFOK, MarketIOC, or MarketFOK.
* **Price:** An unsigned 64-bit integer in atomic currency units (like pence or cents). A limit order must have a price > 0. A market order has no price.
* **Quantity:** An unsigned 64-bit integer. Quantity must be > 0.
* **Engine sequence:** A monotonically increasing unsigned 64-bit integer assigned by the engine to every received command before routing. It starts at 1 and determines arrival order. The sequence must never wrap.

Caller timestamps may be retained as metadata, but must not affect matching or event ordering.

## Command Validation

A new order is rejected without changing any book when it has an unknown symbol, a duplicate previously accepted order ID, a zero quantity, an invalid side or order type, a missing or zero limit price, or a price on a market order.

Arithmetic must not overflow, including when counting available liquidity for FOK orders.

## Matching Semantics

### Price priority

* Buy orders: higher price has priority
* Sell orders: lower price has priority

### Time priority

* Orders at the same price level execute in arrival order (FIFO)

### Trade price

The resting order decides the execution price. For example, if the book ask is 100 and the incoming buy limit is 105, the trade executes at 100.

### Fills and remaining quantity

Limit orders match at their limit price or better. Market orders match at the best available prices on the opposite side and never rest on the book.

* **GTC (limit only):** Fill what is available and rest any remaining quantity.
* **IOC:** Fill what is available and cancel any remaining quantity.
* **FOK:** Fill the entire quantity or reject with `InsufficientLiquidity`, without any trades or book changes. A limit FOK counts only liquidity at its limit price or better; a market FOK counts all liquidity on the opposite side.

### Cancellation

A cancel command identifies its target by order ID; a symbol is unnecessary because order IDs are global.

* Cancelling an active order removes its remaining quantity from the book.
* Cancelling an unknown, filled, already cancelled, or otherwise inactive order produces `CancelRejected` and makes no book changes.

## Command ordering

Commands for the same symbol are processed in engine-sequence order. A command is processed atomically with respect to that symbol: all of its book changes and output events are determined before the next command for the symbol is applied.

Commands for different symbols may execute concurrently. Output ordering is defined in [Command and event interfaces](interfaces.md#event-ordering).
