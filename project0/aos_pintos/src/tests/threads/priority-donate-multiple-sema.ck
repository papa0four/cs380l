# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(priority-donate-multiple-sema) begin
(priority-donate-multiple-sema) Main thread should have priority 32.  Actual priority: 32.
(priority-donate-multiple-sema) Main thread should have priority 33.  Actual priority: 33.
(priority-donate-multiple-sema) Thread b downed semaphore b.
(priority-donate-multiple-sema) Thread b finished.
(priority-donate-multiple-sema) Thread b should have just finished.
(priority-donate-multiple-sema) Main thread should have priority 32.  Actual priority: 32.
(priority-donate-multiple-sema) Thread a downed semaphore a.
(priority-donate-multiple-sema) Thread a finished.
(priority-donate-multiple-sema) Thread a should have just finished.
(priority-donate-multiple-sema) Main thread should have priority 31.  Actual priority: 31.
(priority-donate-multiple-sema) end
EOF
pass;
