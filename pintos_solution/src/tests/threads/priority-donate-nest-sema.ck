# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(priority-donate-nest-sema) begin
(priority-donate-nest-sema) Low thread should have priority 32.  Actual priority: 32.
(priority-donate-nest-sema) Low thread should have priority 33.  Actual priority: 33.
(priority-donate-nest-sema) Medium thread should have priority 33.  Actual priority: 33.
(priority-donate-nest-sema) Medium thread downed the semaphores.
(priority-donate-nest-sema) High thread downed the semaphore.
(priority-donate-nest-sema) High thread finished.
(priority-donate-nest-sema) High thread should have just finished.
(priority-donate-nest-sema) Middle thread finished.
(priority-donate-nest-sema) Medium thread should just have finished.
(priority-donate-nest-sema) Low thread should have priority 31.  Actual priority: 31.
(priority-donate-nest-sema) end
EOF
pass;
