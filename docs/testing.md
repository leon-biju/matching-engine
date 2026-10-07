# Testing

Both versions will have unit and integration tests for the [matching rules](semantics.md) and [command/event contract](interfaces.md). Property/randomised tests and fuzz tests will check the invariants below across generated command streams.

Differential tests will run the same commands through both engines and compare their complete output event streams and final book state.

## Invariants

Check book and quantity invariants after each complete command. Check event ordering as batches are published.

1. No active order has quantity 0.

2. Every active order exists exactly once.

3. Every active-order index entry refers to a valid active order. Previously accepted IDs may remain in lifecycle tracking after becoming inactive.

4. Bid prices are ordered descending.

5. Ask prices are ordered ascending.

6. FIFO is preserved within a price level.

7. `best_bid < best_ask`, unless one side is empty.

8. For every accepted order, its submitted quantity equals its total executed quantity plus its currently resting quantity plus its cancelled quantity.

9. A rejected FOK order produces no trades and no side effects on the book. An accepted FOK order is filled completely.

10. Output events are strictly ordered by engine sequence and then event index, with no interleaving between command batches.

11. An accepted order ID is never accepted a second time during the lifetime of an engine instance.
