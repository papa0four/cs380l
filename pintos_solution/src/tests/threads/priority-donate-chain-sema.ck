# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(priority-donate-chain-sema) begin
(priority-donate-chain-sema) main downed semaphore.
(priority-donate-chain-sema) main should have priority 3.  Actual priority: 3.
(priority-donate-chain-sema) main should have priority 6.  Actual priority: 6.
(priority-donate-chain-sema) main should have priority 9.  Actual priority: 9.
(priority-donate-chain-sema) main should have priority 12.  Actual priority: 12.
(priority-donate-chain-sema) main should have priority 15.  Actual priority: 15.
(priority-donate-chain-sema) main should have priority 18.  Actual priority: 18.
(priority-donate-chain-sema) main should have priority 21.  Actual priority: 21.
(priority-donate-chain-sema) thread 1 downed semaphore
(priority-donate-chain-sema) thread 1 should have priority 21. Actual priority: 21
(priority-donate-chain-sema) thread 2 downed semaphore
(priority-donate-chain-sema) thread 2 should have priority 21. Actual priority: 21
(priority-donate-chain-sema) thread 3 downed semaphore
(priority-donate-chain-sema) thread 3 should have priority 21. Actual priority: 21
(priority-donate-chain-sema) thread 4 downed semaphore
(priority-donate-chain-sema) thread 4 should have priority 21. Actual priority: 21
(priority-donate-chain-sema) thread 5 downed semaphore
(priority-donate-chain-sema) thread 5 should have priority 21. Actual priority: 21
(priority-donate-chain-sema) thread 6 downed semaphore
(priority-donate-chain-sema) thread 6 should have priority 21. Actual priority: 21
(priority-donate-chain-sema) thread 7 downed semaphore
(priority-donate-chain-sema) thread 7 should have priority 21. Actual priority: 21
(priority-donate-chain-sema) thread 7 finishing with priority 21.
(priority-donate-chain-sema) interloper 7 finished.
(priority-donate-chain-sema) thread 6 finishing with priority 18.
(priority-donate-chain-sema) interloper 6 finished.
(priority-donate-chain-sema) thread 5 finishing with priority 15.
(priority-donate-chain-sema) interloper 5 finished.
(priority-donate-chain-sema) thread 4 finishing with priority 12.
(priority-donate-chain-sema) interloper 4 finished.
(priority-donate-chain-sema) thread 3 finishing with priority 9.
(priority-donate-chain-sema) interloper 3 finished.
(priority-donate-chain-sema) thread 2 finishing with priority 6.
(priority-donate-chain-sema) interloper 2 finished.
(priority-donate-chain-sema) thread 1 finishing with priority 3.
(priority-donate-chain-sema) interloper 1 finished.
(priority-donate-chain-sema) main finishing with priority 0.
(priority-donate-chain-sema) end
EOF
pass;
