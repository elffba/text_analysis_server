
çalıştırmak için
Last login: Tue Dec 17 13:18:46 on ttys000
elifbaser@Elif-MacBook-Air-2 ~ % cd c_odev

elifbaser@Elif-MacBook-Air-2 c_odev % docker build -t telnet-container .

elifbaser@Elif-MacBook-Air-2 c_odev % docker run -d --name telnet-container -p 23:23 telnet-container

(eğer kullanımdaysa)
elifbaser@Elif-MacBook-Air-2 c_odev % docker stop telnet-container
docker rm telnet-container
telnet-container
telnet-container

elifbaser@Elif-MacBook-Air-2 c_odev % docker run -d --name telnet-container -p 23:23 telnet-container
8eb059c856223de3162cf37989635e759cf630dd35db62ef8e92cb3accd8a912

elifbaser@Elif-MacBook-Air-2 c_odev % gcc -o telnet_server main.c levenshtein.c -pthread

elifbaser@Elif-MacBook-Air-2 c_odev % ./telnet_server
Telnet sunucusu 60000 portunda dinliyor…

bu yazıyı gördükten sonra yeni bir terminal penceresi açıyoruz ve aşağıdakini yazıyoruz 

telnet 127.0.0.1 60000




bu kısım benimle ilgili ciddiye almayınız
elifbaser@Elif-MacBook-Air-2 c_odev % docker build -t telnet-container .

elifbaser@Elif-MacBook-Air-2 c_odev % docker run -d --name telnet-container -p 23:23 telnet-container
6e5593ce51e2d543d9cf8cbb4185be7ee5e37f398dc2e5df18709d467b2e5e7a

elifbaser@Elif-MacBook-Air-2 c_odev % gcc -o telnet_server main.c levenshtein.c -pthread

elifbaser@Elif-MacBook-Air-2 c_odev % ./telnet_server
Server listening on port 60000...
Connection accepted from 127.0.0.1:59469
Connection closed or error occurred.







