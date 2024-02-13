# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(priority-donate-one-sema) begin
(priority-donate-one-sema) This thread should have priority 32.  Actual priority: 32.
(priority-donate-one-sema) This thread should have priority 33.  Actual priority: 33.
(priority-donate-one-sema) down2: downed the semaphore
(priority-donate-one-sema) down2: done
(priority-donate-one-sema) down1: downed the semaphore
(priority-donate-one-sema) down1: done
(priority-donate-one-sema) down2, down1 must already have finished, in that order.
(priority-donate-one-sema) This should be the last line before finishing this test.
(priority-donate-one-sema) end
EOF
pass;
