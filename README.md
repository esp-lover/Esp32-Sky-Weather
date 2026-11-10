ESP32 Mini Air Station

I was bored, so I decided to make this project!

This project is a mini air station powered by an ESP32-S3 Zero. If you use a different ESP32 board, you may need to change the pin assignments.

I configured the OpenSky API to track aircraft over my country's region. If OpenSky doesn't work properly, you can customize these coordinates in the code:

lamin = 36.0;
lamax = 42.0;
lomin = 26.0;
lomax = 45.0;

These coordinates define the geographical area used to search for aircraft.

Setup:

You need to enter your own API details and Wi-Fi credentials. Without them, the weather and aircraft tracking features won't work. The games will still work!

This project is not perfect. If you find any bugs, please let me know by opening an issue.

Thanks for checking out my project!
