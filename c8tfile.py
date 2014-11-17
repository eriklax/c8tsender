#!/usr/bin/env python
import os, sys, time, httplib, json

uuid = None
queue_only = queue_only if 'queue_only' in locals() else False

def addtoplaylist(file):
	global uuid
	# feel free to suggest more
	movieExtensions = [
			'.mkv', '.mp4', '.mpg', '.mpeg', '.avi'
		]
	if not os.path.splitext(file)[1] in movieExtensions:
		return
	conn = httplib.HTTPConnection('127.0.0.1', 8080)
	conn.request('POST', '/playlist', file)
	resp = conn.getresponse()
	if not uuid:
		if resp.status == 200:
			try:
				data = resp.read()
				uuid = json.loads(data)['uuid']
			except:
				pass
	conn.close()

def tree(path):
    for f in os.listdir(path):
        f = os.path.join(path, f)
        if os.path.isfile(f):
            addtoplaylist(f)
        if os.path.isdir(f):
            tree(f)

if __name__ == '__main__':
    # don't autostart when folders are queued
    playFile = False
    for f in sys.argv[1:]:
       f = os.path.realpath(f)
       if os.path.isfile(f):
           addtoplaylist(f)
           playFile = True
       if os.path.isdir(f):
           tree(f)
    if uuid and playFile and not queue_only:
        conn = httplib.HTTPConnection('127.0.0.1', 8080)
        conn.request('GET', '/play/' + uuid)
        conn.getresponse()
        conn.close()
