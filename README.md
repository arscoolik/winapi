# winapi
**Arseniy Sitnikov MIPT 7 semester**


Running on your device:

Clone:
    `git clone https://github.com/arscoolik/winapi.git`
Enter the directory:
   ` cd winapi`
Build:
    `gcc -o server server.c -lws2_32 -luser32 -lmswsock -ladvapi32 -mwindows`
    `gcc -o client client.c -lws2_32 -luser32 -lmswsock -ladvapi32 -mwindows`
Run:
    `./server`
    `./client <ipv4 addr>`
