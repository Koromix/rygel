Output start.gif
Set FontSize 14
Set Width 800
Set Height 200
Set Padding 20
Set BorderRadius 10
Set WindowBar ColorfulRight

Env LANG C

Type 'ls -lh /home/niels/projects'
Sleep 500ms
Enter
Sleep 2s

Type 'rekkord setup'
Sleep 500ms
Enter
Sleep 1s
Enter
Sleep 1s
Enter
Sleep 1s
Enter
Sleep 1s
Type '/home/niels/repo'
Enter
Sleep 2s

Type 'rekkord init'
Sleep 500ms
Enter
Sleep 2s

Type 'rekkord save projects /home/niels/projects'
Sleep 500ms
Enter
Wait@60s
Sleep 2s

Type 'rekkord snapshots'
Sleep 500ms
Enter
Sleep 4s

Type 'rekkord list projects'
Sleep 500ms
Enter
Sleep 4s

Type 'rekkord restore projects /tmp/restore'
Sleep 500ms
Enter
Wait@60s
Sleep 2s

Type 'ls -lh /tmp/restore/home/niels/projects'
Sleep 500ms
Enter
Sleep 4s

Sleep 2s
