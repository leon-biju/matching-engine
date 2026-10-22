# Matching semantics
This document defines the order model and matching rules for both engines. Both engines must produce identical output for the same input.

## Order model
* **Order ID:** An unsigned 64-bit integer. IDs are global across all symbols. Once an order has been accepted, its ID cannot be reused during the lifetime of that engine instance, including after the order is filled or cancelled. An ID from a rejected order may be submitted again.
* **Symbol:** An instrument identifier from the fixed set passed to the engine constructor. There is no command to add a symbol (yet), and orders for any other symbol are rejected with `UnknownSymbol`. Symbols are matched exactly and are case-sensitive.
* **Side:** Buy or Sell.
* **Order type:** LimitGTC, LimitIOC, LimitFOK, MarketIOC, or MarketFOK. See [Order types](#order-types).
* **Price:** An unsigned 64-bit integer in atomic currency units (like pence or cents). A limit order must have a price > 0. A market order must not have a price.
* **Quantity:** An unsigned 64-bit integer. Quantity must always be > 0.
* **Engine sequence:** A monotonically increasing unsigned 64-bit integer assigned by the engine to every received command before routing. It starts at 1 and determines arrival order. Every command uses up one sequence number, including commands that are rejected. The sequence must never wrap.

## Validation
A `NewOrder` command is checked against the following constraints, and is rejected with the first reason that applies. A rejected order produces no other events and never touches the book.

1. `DuplicateOrderId`: the ID belongs to an order that has already been accepted.
2. `UnknownSymbol`: the symbol is not in the engine's symbol set.
3. `InvalidQuantity`: quantity == 0 (since qty is uint64_t don't need to consider negative values).
4. `InvalidPrice`: a limit order has no price or a price of 0, or a market order has a price.
5. `NoLiquidity`: a market order arrives when the opposite side of the book is empty.

## Matching rules
* **Price-time priority:** An incoming order matches against the best price on the opposite side first. So the lowest sell price for a buy, and the highest buy price for a sell. In the same price level, the order with the earliest engine sequence is filled first.
* **Crossing:** A buy limit order can match sells priced at or below its limit. A sell limit order can match buys priced at or above its limit. A market order can match at any price.
* **Trade price:** Trades always execute at the resting order's price.
* **Fills:** Matching continues level by level until either the incoming order is filled, or no remaining resting order crosses. Each resting order that is matched produces one `Trade` for the quantity taken from it. A partially filled resting order keeps its place in the queue.
* **Self-trade:** Since orders carry no account or owner, so the engine cannot tell whether both sides of a trade belong to the same participant. There is no self-trade prevention, and any buy may match any sell.

## Order types
* **LimitGTC (good till cancelled):** Matches as much as it can on arrival. Any remainder rests on the book at its limit price until it is filled or cancelled.
* **LimitIOC (immediate or cancel):** Matches as much as it can on arrival. Any remainder is cancelled and never rests.
* **LimitFOK (fill or kill):** Fills completely on arrival or not at all. Before any trade happens, the engine checks whether enough crossing quantity is resting. If there is, the order matches like an IOC and is completely filled. If there is not, no trades happen and the whole order is cancelled.
* **MarketIOC:** Like LimitIOC with no price limit. It is rejected with `NoLiquidity` if the opposite side is empty.
* **MarketFOK:** Like LimitFOK with no price limit. It is rejected with `NoLiquidity` if the opposite side is empty, and is cancelled if the opposite side does not hold enough quantity.

So really, only LimitGTC orders ever rest on the book.

## Cancels
* Only resting orders can be cancelled. Cancelling a partially filled order only removes the unfilled remainder. The trades that have already happened cannot be undone.
* A cancel for an order that is not resting is rejected with `UnknownOrder`. This covers all IDs that were never accepted, orders that are already filled/cancelled, and IOC or FOK orders, which never rest in the book.
* A cancel only needs to name the order ID. It does not need the symbol, because IDs are global.
