# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(priority-donate-lower-sema) begin
(priority-donate-lower-sema) Main thread should have priority 41.  Actual priority: 41.
(priority-donate-lower-sema) Lowering base priority...
(priority-donate-lower-sema) Main thread should have priority 41.  Actual priority: 41.
(priority-donate-lower-sema) down: downed the semaphore
(priority-donate-lower-sema) down: done
(priority-donate-lower-sema) down must already have finished.
(priority-donate-lower-sema) Main thread should have priority 21.  Actual priority: 21.
(priority-donate-lower-sema) end
EOF
pass;
