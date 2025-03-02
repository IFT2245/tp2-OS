# TP2-OS
## CREATED BY SPOKBOUD


```sh
echo "3\n1\n6\n" | valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --trace-children=yes --child-silent-after-fork=no ./main 2&> ../../output-valgrind.txt
```
            READY_QUEUE SCHEMAS
            ─────────────────────
```rust
F   I   F   O
┌────────────────┐
│   [ head ]     │
│    p1 -> p2 -> p3 -> NULL
└────────────────┘
   Enfilage: queue.
   Défilage: tête.

P  R  I  O  R  I  T  I  E  S
┌────────────────┐
│   [head]       │
│   insère trié  │
└────────────────┘
   Ordre asc de la priorité:
     pX.prio < pY.prio  => pX avant

S   J   F
┌────────────────┐
│   [head]       │
│   burst asc    │
└────────────────┘
   Insère process avec burst_time plus petit avant.

R   R
┌─────────────┐
│   [head]     │
│   Quantum    │
└─────────────┘
   Tourniquet:
     pop front -> exécute q ms -> si pas fini -> push back

B   F   S
┌─────────────────┐
│   [head]        │
│   FIFO semblable │
└─────────────────┘
   Mais saute les arrivées en "Breadth"


M   L   F   Q
┌──── Q0 ────┐
│   q=2 ms   │
└────────────┘
┌──── Q1 ────┐
│   q=4 ms   │
└────────────┘
┌──── Q2 ────┐
│   q=6 ms   │
└────────────┘
   Process preempté level++

W   F   Q
┌─────────────────────────┐
│ Weighted Fair Queueing  │
│   finish_time ~ Vt + (remain/weight)
└─────────────────────────┘
   Chercher min finish_time à chaque pop.

H   P   C   BFS
( 0 main cores => HPC gère tout )
┌─────────────────────────┐
│ HPC threads => BFS queue│
│ main procs + HPC procs  │
└─────────────────────────┘
   HPC peut "voler" process si allow_hpc_steal=true.
```

WORKER THREADS LOGIC
────────────────────
1) rq_pop() -> run_slice() -> record timeline -> rq_push() si reste.
2) check_arrivals() insère si arrival_time <= temps actuel.
3) preempt signal => siglongjmp => slice stoppé.
4) if queues vides & arrêts restants => sim_time avance.

CONCURRENCY TIPS
────────────────
• Protéger rq->head / rq->size avec mutex.
• Attendre condition si vide.
• HPC_THREADS ARE CORE_IDS
• Pousse ta file dans la file-hpc si tu n'as pas de coeurs principaux pour le bonus

TESTS (OPTION 5, X10)
─────────────────────
• Vérifier remaining_time=0 partout.
• Aucune fuite mémoire.
• **Passer 10 exécutions de suite** pour le grading final (option 5)
• Autograder à venir

---
## Librairie
Le timeline est discret, et la préemption possible après les courses partielles, 


```rust
  START
    |
    |
    v
init_preempt_timer()
 | (sigaltstack, sigaction)
 | (setitimer(1ms))
 v
[ SIGALRM ] 
   -> preempt_signal_handler()
        -> (siglongjmp(jmpbuf[coreId]))

    block_preempt_signal()    unblock_preempt_signal()
           (bloque)  <---->   (autorise)

disable_preempt_timer()
 | (setitimer(0),
 |  sigaction par défaut,
 |  free altstack)
 v
Fin
```

```c
sigjmp_buf env;
if (sigsetjmp(env, 1) == 0) {
    register_jmpbuf_for_core(coreId, env);
    // ... code initial ...
} else {
    // ... on revient ici après siglongjmp ...
    //    donc préemption effectuée
}
```