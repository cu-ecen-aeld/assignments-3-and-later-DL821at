Welcome to Buildroot
buildroot login: root
login[110]: root login on 'console'
# lsmod
Module                  Size  Used by    Tainted: G  
hello                  12288  0 
faulty                 12288  0 
scull                  24576  0 
# echo "hello_world" > /dev/faulty
[  344.275626] Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
[  344.275792] Mem abort info:
[  344.275831]   ESR = 0x0000000096000044
[  344.275876]   EC = 0x25: DABT (current EL), IL = 32 bits
[  344.275944]   SET = 0, FnV = 0
[  344.275987]   EA = 0, S1PTW = 0
[  344.276034]   FSC = 0x04: level 0 translation fault
[  344.276105] Data abort info:
[  344.276149]   ISV = 0, ISS = 0x00000044, ISS2 = 0x00000000
[  344.276217]   CM = 0, WnR = 1, TnD = 0, TagAccess = 0
[  344.276287]   GCS = 0, Overlay = 0, DirtyBit = 0, Xs = 0
[  344.276385] user pgtable: 4k pages, 48-bit VAs, pgdp=00000000437b3000
[  344.276464] [0000000000000000] pgd=0000000000000000, p4d=0000000000000000
[  344.276607] Internal error: Oops: 0000000096000044 [#1] PREEMPT SMP
[  344.276764] Modules linked in: hello(O) faulty(O) scull(O)
[  344.277010] CPU: 0 PID: 110 Comm: sh Tainted: G           O       6.6.50 #1
[  344.277147] Hardware name: linux,dummy-virt (DT)
[  344.277304] pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
[  344.277407] pc : faulty_write+0x8/0x10 [faulty]
[  344.277686] lr : vfs_write+0xc8/0x30c
[  344.277750] sp : ffff80008023bd20
[  344.277797] x29: ffff80008023bd80 x28: ffff1442837c8ec0 x27: 0000000000000000
[  344.277917] x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
[  344.278017] x23: 0000000000000000 x22: ffff80008023bdc0 x21: 0000aaaaf1a2c9e0
[  344.278117] x20: ffff144283799200 x19: 000000000000000c x18: 0000000000000000
[  344.278212] x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
[  344.278307] x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
[  344.278400] x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
[  344.278504] x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
[  344.278596] x5 : 0000000000000000 x4 : ffffb4e0383ca000 x3 : ffff80008023bdc0
[  344.278690] x2 : 000000000000000c x1 : 0000000000000000 x0 : 0000000000000000
[  344.278852] Call trace:
[  344.278960]  faulty_write+0x8/0x10 [faulty]
[  344.279090]  ksys_write+0x74/0x10c
[  344.279149]  __arm64_sys_write+0x1c/0x28
[  344.279202]  invoke_syscall+0x48/0x118
[  344.279255]  el0_svc_common.constprop.0+0x40/0xe0
[  344.279318]  do_el0_svc+0x1c/0x28
[  344.279365]  el0_svc+0x40/0xe8
[  344.279411]  el0t_64_sync_handler+0x100/0x12c
[  344.279472]  el0t_64_sync+0x190/0x194
[  344.279701] Code: ???????? ???????? d2800001 d2800000 (b900003f) 
[  344.279880] ---[ end trace 0000000000000000 ]---

Welcome to Buildroot
buildroot login: 

