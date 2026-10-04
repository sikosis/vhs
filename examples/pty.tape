# PTY and text-output example for Haiku
Output haiku-session.txt

Set Shell bash
Set Columns 80
Set Rows 24
Set TypingSpeed 25ms
Set WaitTimeout 5s

Env VHS_READY PTY_READY
Env VHS_DONE PTY_COMPLETE

Type `printf 'VHS for Haiku on '; uname -s`
Enter
Wait+Screen /Haiku/

Type `echo $VHS_READY`
Enter
Wait+Screen /PTY_READY/

Hide
Type@0ms `echo this-section-is-hidden`
Enter
Sleep 250ms
Show

Type `echo $VHS_DONE`
Enter
Wait+Screen /PTY_COMPLETE/
Sleep 250ms
