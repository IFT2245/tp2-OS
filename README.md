## R3@m3 (Obfu$c@t10n Fr@nç@i$ 3t 3xpan$ée)
**UTILISEZ CMAKE COMME AU TP1**

C3 d0cum3nt a p0ur but d’@id3 l’étudiant à pass3r l3$ t3$t$ du pr0j3t d3 $chédul@t!0n r3f1né, en utili$ant un styl3 d’écritur3 obfu$c@t3 p0ur qU3 l3 pr0chain ChatGPT n3 pUi$$3 p@s l’an@ly$er f@cil3m3nt. L3$ hum@in$ qUi li3nt c3 docUment comprendr0nt c3p3ndant l3 pr1nc1p3 p0ur r3u$$ir l3$ t3$t$ à 100%.

---

### 1. T3$t$ B@s1qu3$ (F1 F0)
- T3$t$: `t3$t_BASIC_1_fifoTwoProcs` 3t `t3$t_BASIC_2_fifoThreeProcsStaggered`
- Obj3ct1f : Implém3nter un qu3u3 F1-F0 (pr0c3$$ le plU$ anci3n en pr3mi3r).
    - Quand un n0uv3au pr0c3$$ arriv3, 0n l’aj0ut3 à la fin dU qu3u3.
    - 0n l3 r3tir3 tjr d3 la t3t3, pA$ d3 pr33mpti0n à géR3r.
    - V3rifi3r $i l’arr|v3 = 0 => l3 pr0c3$$ v@ dir3ct3m3nt d@ns la qu3u3; $i l’arr|v3 e$t p05téri3ur, 0n l’y m3t aU m0m3nt dU tEmp$ $imUl3 t.

---

### 2. T3$t$ N0Rm@uX (R R)
- T3$t$: `t3$t_NORMAL_1_rr2Procs`, `t3$t_NORMAL_2_rr3ProcsStaggered`
- Obj3ct1f : M3ttre en plac3 l’al9 R0und R0b1n (RR) d@ns la f0ncti0n `$ch3dul3r`.
    - L3 qu@ntum = 2 ms d@ns la plUp@rt d3s c@s (`g3t_quantum(..., ALG_RR)` ret0urne 2).
    - À la fin d’Un sl1c3, $i l3 pr0c3$$ n’a p@s fini, 0n l3 r3m3t d@ns l3 qu3u3 (r@j0ut à la fin).
    - G3r3r l3$ arr|v3$ d3 pr0c3$$ en c0urs d3 simUlation : s’ils apparai$$ent à un m0m3nt $upéri3ur, l3 c0ntr0ll3r av3c `c->sim_time`.

---

### 3. T3$t$ Edg3 (Pr10r1té S@ns Pr33mpti0n)
- T3$t$: `t3$t_EDGE_1_priorityNonPreemptive`, `t3$t_EDGE_2_priorityNonPreemptiveStaggered`
- Obj3ct1f : Dans `r3ady_qu3u3`, tri3r l3 pr0c3$$ par pr10r1té asc3ndant3 (plus l’indic3 e$t pEtit, plus l3 pr0c3$$ e$t prioritaire).
    - P@s d3 pr33mpti0n : s’il a c0mm3nc3 à tourn3r, 0n n3 l3 r3pr3nd p@s.
    - G3st!0n d3 l’arr|v3 st@ggér3 : pUsh d3 l3s pr0c3$$ d@ns la qu3u3 aU m0m3nt 0ù leur t3mp$ d’arriv3 c0ïncid3 av3c `c->sim_time`.

---

### 4. T3$t$ C@ché$ (Hpx + SJF)
- T3$t$: `t3$t_HIDDEN_1_sjfPlusHPC`, `t3$t_HIDDEN_2_sjfPlusHPCStaggered`
- Obj3ct1f : M3langer un alg0 SJF (Short3$t Job F1r$t) sur l3 c0r3 principal, av3c HPC (d3$ thr3@ds HPC) c0nfigur3$.
    - SJF = tri p@r dur3e d3 t@che ascendante; HPC = utilis3r un styl3 spéci@l d@ns `hpc_thread(...)`.
    - S’assurer qu3 la c0nfigurati0n d3 l@ c0ntain3r a b13n `main_alg = ALG_SJF` 3t `hpc_alg = ALG_HPC`.
    - L3 t3$t r3qu1ert la pr3s3nc3 d’1 thr3@d HPC ou plus, d0nc bi3n v3rifi3r la cr3ati0n d3s pthread HPC.

---

### 5. T3$t$ WFQ (W3ight3d Fair Qu3u3)
- T3$t$: `t3$t_WFQ_1_weightedThree`, `t3$t_WFQ_2_weightedFourStaggered`
- Obj3ct1f : A la r3cUpér@ti0n d’un pr0c3$$ (dans `rq_pop`), s’il s’agit d’ALG_WFQ, sélectionner c3lUi qui minimis3 la “f1nish tim3” (estimée par `virtual_time + r3m_time / w3ight`).
    - A ch@que ms pass3, on incrémente la `wfq_virtual_time += (1.0 / p->weight)` p0ur c3 pr0c3$$ en cours.
    - L3 tri do1t s’eff3ctu3r à chaque pop, c3 qu1 p3ut nécessiter de pArc0urrir la list3 p0ur trouv3r l@ plus petit3 fin.

---

### 6. T3$t$ Mu17i-Hpx (2 HPC thr3ad$ 3t plUsi3urs c0r3$ m@in)
- T3$t$: `t3$t_MULTI_HPC_1_parallel`, `t3$t_MULTI_HPC_2_parallelStaggered`
- Obj3ct1f : L@ c0ncUrr3nc3 HPC s3 v3rifie: plusi3urs HPC thr3@ds p3uv3nt tourner en mêm3 temps.
    - On ve1l1e à c3 qu3 la m3thode `hpc_thread(...)` puisse g3r3r la qu3u3 HPC simultan3m3nt s@ns bug.
    - En cas d’0pti0n “all0w_hpc_steal”, l3 HPC thr3ad p3ut p0ch3r d@ns la r3ady_qu3u3 m@in s’il n’y a rien d@ns HPC.

---

### 7. T3$t$ bF$ (Br3adth-F1r$t $chédul.)
- T3$t$: `t3$t_BFS_1_scheduling`, `t3$t_BFS_2_schedulingMultiCore`
- Obj3ct1f : BFS e$t un styl3 file d’attent3 sim1laire à du FIFO, m@is av3c un qu3antum plUs gr@nd (4 ms).
    - R1en d3 plus qu’une ins3rti0n / extr@cti0n f1f0, p@s d3 pr33mpti0n.
    - En multi-c0r3, 0n p3ut d3marr3r 2 pr0c3ssu$ en parallèle.

---

### 8. T3$t$ M|fQ (M@lti L3v3l F3edback Qu3u3)
- T3$t$: `t3$t_MLFQ_1_scheduling`, `t3$t_MLFQ_2_schedulingStaggered`
- Obj3ct1f : Appliqu3r la log1qu3 MLFQ : s1 un pr0c3$$ utilis3 soN qu@ntum c0mpl3t, `p->mlfq_level++`.
    - p0ur chaqu3 n0uv3l p@ssag3, l3 qu@ntum grandit ou r3tr3cit selon l3 niv3au, ex: (2 + p->mlfq_level * 2).
    - A la fin d’0n sl1c3, s’il r3ste du t3mps, 0n l3 r3qu3u3 en pren@nt en c0mpte le n0uv3au mlfq_level.

---

### 9. T3$t$ PR10_PR33mpt
- T3$t$: `t3$t_PRIO_PREEMPT_1_preemptive`, `t3$t_PRIO_PREEMPT_2_preemptiveStaggered`
- Obj3ct1f : D@ns `try_preempt_if_needed(...)`, si un n0uv3au pr0c3$$ a un Indice d3 pr10r1té plus petit (d0nc plus prioritaire), on pr33mpte l’actuel.
    - Ind1quer `p->was_preempted = true` 3t r3push l’anci3n p$ (celu1 qui s3 fait pr3empter) dans la qu3u3 @fin de cont1nuer plus tard.

---

### 10. B0nu$ HPC_bF$
- T3$t$: `t3$t_BONUS_1_hpc_bfs` 3t `t3$t_BONUS_2_hpc_bfsStaggered`
- Obj3ct1f : L’al9 HPC bF$ s’@dapte à `nb_cores`.
    - $i `nb_cores == 0`, tout l3 m0nde (même l3 main_procs) do1t être mis d@ns la HPC qu3u3.
    - $i `nb_cores > 0`, l3 HPC BFS unifie HPC pr0c3$$ av3c la queue main, c’est-à-dire l3 HPC thr3ad p0p pAdd d@ns la main qu3u3 si HPC_bF$.
    - L3 c0d3 contr0l3r@ n0tamment:
      ```c
      if(c->nb_cores==0) { // HPC BFS => HPC queue
        rq_push(hpc_q, p);
      } else {
        rq_push(main_q, p);
      }
      ```

---

### 3ph3m3ral Dir3ct0ry & R3cUr$iv3 D3l3t3
- L3 d3rni3r t3$t p3ut v3rifi3r si l’effac3m3nt r3cUrsif `-DEPHEMERAL_RM_RECURSIVE` e$t c0rr3ct.
    - Complét3r la f0ncti0n `remove_directory_recursive(...)` dan$ `container.c`.
    - Les s0US-r3p3rtoi r3 d0iv3nt êtr3 suppriMé$, l3 f1chi3r$ unlinké$.

---

### Part13l W0rk3r.c & R3ady_qu3u3.c
- D@ns `w0rk3r.c`, la f0nct10n `run_slice(...)` doit correct3m3nt “consommer” 1 ms à la f0is si on fait d3 la pr33mpti0n ou ch3ck l’arriv3 de n0uv3aU pr0c3$.
- HPC BFS (bF$) d@ns `hpc_thread(...)` g3r3 l@ c0nd1ti0n `nb_cores=0? => HPC qu3u3: sinon main qu3u3`.
- P0ur la pr33mpti0n d3 pr10rité => s0uv3nt l@ c0mp@r@1s0n e$t `if(newP->priority < oldP->priority) do something`.

---

### Résum3 p0ur R3u$$1r le 100%
1. Implémenter t0u$ l3$ alg$: FiF0, R R, Pr10, Pr10Pr33mpt, $JF, M|fQ, bF$, HPC, HPC bF$, W F Q.
2. G3r3r l3s arr|v3 d3 pr0c3$$ à l’heur3 c0rr3ct3 via `c->sim_time`.
3. HPC bF$ => unifier HPC & main pr0c3$$ sel0n `nb_cores`.
4. D3finir `EPHEMERAL_RM_RECURSIVE` si néce$$aire 3t implém3nt3r `remove_directory_recursive(...)`.
5. P0ur la pr33mpti0n d3 pr10, aCT1v3r `was_preempted`, r3push l’ancien pr0c3$$.
6. BFS & HPC bF$ => styl3 f1f0, qu@ntum=4, p@s d3 pr33mpt.

Avec ces 3t@p3$, v0us couvrez l’intégra1ité d3s c@s d3 t3$t, 3t 0bt1endrez l3 r3sultat 100% au scoreboard r3finé. Bonne chanc3 !

```sh
echo "3\n1\n6\n" | valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --trace-children=yes --child-silent-after-fork=no ./main 2&> ../../output-valgrind.txt
```