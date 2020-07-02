#!/usr/bin/env python

import socket
import sys
import signal
import serial
import time
import os
from time import sleep
from threading import Thread

ip   = '10.0.7.233'
port = 1234

def handler(signum, frame):
	print "RES: timeout "
	sys.stdout.flush()
	os._exit(1)

def getline(s):
	ln=0
	msg=''
	chunk='a'
	while chunk != '\n':
		chunk=s.recv(1)
		if chunk == '':
			raise RuntimeError, "socket connection broken"
		#if (chunk != '\n') and (ord(chunk) != 0):
		if (chunk != '\n'):
			msg=msg+chunk
			ln = ln + 1
			#print "Znak: " +str(ord(chunk))
	#print "Recv: <" + msg+" ("+str(ln)+")>"
	return msg


def test_recv(name,port):
	try:
		s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	except socket.error, msg:
		print 'Failed to create socket. Error code: ' + str(msg[0]) + ' , Error message : ' + msg[1]
		sys.stdout.flush()
		os._exit(1)
	print 'Recv: Socket Created'
	try:
		s.connect((ip, port)) 
	except Exception, e:
		print("Recv: Something's wrong with %s. Exception type is %s" % ((ip, port), e))
		sys.stdout.flush()
		os._exit(1)

	print 'Recv: Socket Connected'
	s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
	c = 1
	while True:
		tm = getline(s)
		v = int(tm, 16)
		c = v + 1
		n = '%x' % c;
		#sleep(0.001)
		s.sendall( n + '\n' )
		print tm+"=> "+n+" ("+str(len(n))+")"
	s.close()

def test_send(name,port):
	ser = serial.Serial()
	ser.port = "/dev/ttyUSB0"
	#ser.port = "/dev/ttyS0"
	ser.baudrate = 2000000
	ser.bytesize = serial.EIGHTBITS #number of bits per bytes
	ser.parity   = serial.PARITY_NONE #set parity check: no parity
	ser.stopbits = serial.STOPBITS_TWO #number of stop bits
	#ser.timeout = None          #block read
	ser.timeout = 1            #non-block read
	#ser.timeout = 2              #timeout block read
	ser.xonxoff = False     #disable software flow control
	ser.rtscts = False     #disable hardware (RTS/CTS) flow control
	ser.dsrdtr = False       #disable hardware (DSR/DTR) flow control
	ser.writeTimeout = 2     #timeout for write
	ser.open()
	time.sleep(0.1);
	if ser.isOpen():
		print 'Send: Uart ready'
		try:
			ser.flushInput()
			ser.flushOutput()
			c = 0
			ser.write('%x' % c+'\n')
			c = 1
			while True:
			#for x in range(100):
				tm=ser.readline()
				v = int(tm, 16)
				if v != c:
					print "Send: Error!!! "+hex(v)+" "+hex(c);
					print "RES: ERROR ";
					sys.stdout.flush()
					os._exit(1)
				c = v + 1
				n = '%x' % c
				#sleep(0.0001)
				ser.write( n +'\n' )
				c = c + 1
			ser.close()
			print "RES: OK ";
			sys.stdout.flush()
			os._exit(0)
		except Exception, e1:
			print "error communicating...: " + str(e1)
			sys.stdout.flush()
			os._exit(0)

#signal.signal(signal.SIGALRM, handler)
#signal.alarm(5)

t = Thread(target=test_recv, args=("",port))
ts = Thread(target=test_send, args=("",port))
t.start()
ts.start()

while 1:
	pass

