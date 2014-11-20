c8t sender is a Google Chromecast sender server with a built-in REST API and responsive playlist interface. Sound and video remuxing (encoding) is done with ffmpeg.

Requirements
------------
* It's a C++11 project (go figure)
* libmicrohttpd (gnu.org)
* ffmpeg (http://ffmpeg.org/)

Installation
------------
c8tsender requires `ffmpeg` in the $PATH or $PWD (in the same directory) in order to remux files to mkv, and convert the sound to aac), the flags to `ffmpeg` are not in away way optimized for you, but they worked for me.

Bonus: Install shell extension in OSX
-----------------------------------
In `Automator` create a new `Service`.

1. Service receives selected `files or folders` in `Finder`.
2. Add a new `Run Shell Script` action, us Shell `/usr/bin/python`, Pass input `as arguments`.
3. Paste the content below, but correct the path for your `c8tfile.py`.

```
#!/usr/bin/env python

queue_only = False # or True
execfile("/Users/erik/castaway/c8tfile.py")
```

4. Save with a useful name, such as 'Play on Chromecast' or 'Queue on Chromecast'

In `Finder` a new context menu option is available, select a file or folder and play on Chromecast!

Running
-------

1. Run the CastAway server.
   
   `./c8tsender 192.168.1.78` # or whatever IP your Chromecast has

3. Run `python c8tfile.py /path/to/file/or/folder` to begin queuing files (or use shell extension mentioned above).

4. Open `http://127.0.0.1:8080` (or LAN-IP) to control the playback using any browser/device.
