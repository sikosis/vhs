# Animated-output example for Haiku
Output animation-demo.txt
Output animation-demo.gif
Output animation-demo.mp4
Output animation-demo.webm

Set Shell bash
Set Columns 52
Set Rows 14
Set Width 960
Set Height 540
Set FontSize 24
Set FontFamily "Noto Sans Mono"
Set Theme "Dracula"
Set Padding 28
Set Margin 24
Set MarginFill "#44475a"
Set WindowBar Colorful
Set BorderRadius 12
Set Framerate 24
Set PlaybackSpeed 1.25
Set LoopOffset 10%
Set TypingSpeed 22ms
Set WaitTimeout 5s

Hide
Type@0ms `stty -echo`
Enter
Sleep 100ms
Type@0ms `clear`
Enter
Sleep 100ms
Show

Type@0ms `printf '\033[1;35mAnimated VHS for Haiku\033[0m\n'`
Enter
Sleep 750ms
Type@0ms `printf '\033[36mGIF  •  MP4  •  WebM\033[0m\n'`
Enter
Sleep 750ms
Copy "echo clipboard-from-vhs"
Paste
Enter
Wait+Screen /clipboard-from-vhs/
Sleep 1s
