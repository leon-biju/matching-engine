# Matching semantics
This document defines the order model and matching rules for both engines.

## Order model
* **Order ID:** An unsigned 64-bit integer. IDs are global across all symbols. Once an order has been accepted, its ID cannot be reused during the lifetime of that engine instance, including after the order is filled or cancelled. An ID from a rejected order may be submitted again.
* **Symbol:** An instrument identifier from the fixed set passed to the engine constructor. There is no command to add a symbol (yet), and orders for any other symbol are rejected with `UnknownSymbol`. Symbols are matched exactly and are case-sensitive.
* **Side:** Buy or Sell.
* **Order type:** LimitGTC, LimitIOC, LimitFOK, MarketIOC, or MarketFOK.
* **Price:** An unsigned 64-bit integer in atomic currency units (like pence or cents). A limit order must have a price > 0. A market order must not have a price.
* **Quantity:** An unsigned 64-bit integer. Quantity must always be > 0.
* **Engine sequence:** A monotonically increasing unsigned 64-bit integer assigned by the engine to every received command before routing. It starts at 1 and determines arrival order. The sequence must never wrap.
