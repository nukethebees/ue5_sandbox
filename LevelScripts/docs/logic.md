# Level logic

Declare expressions and rules inside one `:logic (logic ...)` block. Scheme builds the rules at load time. The simulation samples them after deaths each tick.

```scheme
:logic (logic
  (define deadline (timer))
  (initially (start-timer deadline 30))
  (on :when (timer-elapsed deadline) :do (stop)))
```

- `initially` dispatches setup before the first tick. Timers count simulation time and pause with the level.
- `on` fires once, on its first true sample. A latch captures once and publishes the value next tick.
- `spawn` queues a declared spawn group for Preparation. Other effects run at tick end in queue order. The first terminal effect sets the result.
- Use `win-race` for competing objectives. Success wins a same-tick tie. [Logic helpers](../Libraries/logic.scm) · [Playable example](../BorderSkirmish/BorderSkirmish.scm)
