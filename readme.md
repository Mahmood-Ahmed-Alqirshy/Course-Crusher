# Crusher
is an app to split long courses into parts by providing content file with the following format:
</br>
</br>
00:00:00 Video title 1
</br>
00:01:14 Video title 2
</br>
00:05:48 Video title 3
</br>
00:09:27 Video title 4
</br>
00:16:03 Video title 5
</br>

## Environment :
OS : Debian 11
</br>
FFMPEG : v4.3.6
</br>
NodeJS : v12.22.12
</br>
GCC : v10.2.1

## How to use :
compile the code and name the output "crusher" and then run the following command
</br>
```
./crusher <video file> <content file> <output dir>
```