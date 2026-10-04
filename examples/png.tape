# Native PNG example for Haiku
Output png-demo.txt
Output png-demo.png
Output png-demo-frames/

Set Shell bash
Set Columns 48
Set Rows 14
Set Width 1000
Set Height 560
Set FontSize 26
Set FontFamily "Noto Sans Mono"
Set Padding 28
Set Margin 24
Set MarginFill "#24273a"
Set WindowBar Colorful
Set BorderRadius 12
Set TypingSpeed 15ms
Set WaitTimeout 5s

Env VHS_COLOUR_READY COLOUR_READY
Hide
Type@0ms `stty -echo`
Enter
Sleep 100ms
Type@0ms `clear`
Enter
Sleep 100ms
Show

Type@0ms `printf '\033[1;36mVHS for Haiku\033[0m\n'`
Enter
Wait+Screen /Haiku/
Type@0ms `printf '\033[32mANSI green\033[0m  \033[38;2;255;175;95mtrue colour\033[0m\n'`
Enter
Type@0ms `printf 'UTF-8: café 世界 📼\n'`
Enter
Type@0ms `echo $VHS_COLOUR_READY`
Enter
Wait+Screen /COLOUR_READY/
Screenshot png-midpoint.png
Sleep 500ms
