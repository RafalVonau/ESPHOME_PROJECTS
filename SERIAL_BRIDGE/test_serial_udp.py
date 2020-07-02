#!/usr/bin/env python

import socket
import sys
import signal
import serial
import time
import os
from time import sleep
from threading import Thread

packet_size=100
ip   = '10.0.7.233'
port = 1234

def handler(signum, frame):
	print "RES: timeout "
	sys.stdout.flush()
	os._exit(1)

def getline(s):
	data, addr = s.recvfrom(4096)
	return data

def test_recv(name,port):
	try:
		s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
	except socket.error, msg:
		print 'Failed to create socket. Error code: ' + str(msg[0]) + ' , Error message : ' + msg[1]
		sys.stdout.flush()
		os._exit(1)
	print 'Recv: Socket Created'
	try:
		s.bind(('10.0.7.25', 38000 + port))
	except Exception, e:
		print("Recv: Something's wrong with %s. Exception type is %s" % (('10.0.7.25', port), e))
		sys.stdout.flush()
		os._exit(1)

	print 'Recv: Socket Connected'
	c = 1
	s.sendto( '\n', (ip,port ))
	#getline(s)
	while True:
		tm = getline(s)
		#print 'Recv: Got line ' +tm;
		v = int(tm[-20:], 16)
		c = v + 1
		n = '%x' % c;
		vmsg = '%x' % v;
		#sleep(0.001)
		k='0'*(packet_size-1-len(n));
		msg=k + n + '\n'
		s.sendto(msg , (ip ,port ))
		#print vmsg+"=> "+n+" ("+str(len(msg))+")"
		print tm+"=> "+n+" ("+str(len(msg))+")"
	s.close()

def test_send(name,port):
	ser = serial.Serial()
	ser.port = "/dev/ttyUSB0"
	#ser.port = "/dev/ttyS0"
	ser.baudrate = 2000000
	ser.bytesize = serial.EIGHTBITS #number of bits per bytes
	ser.parity = serial.PARITY_NONE #set parity check: no parity
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
			#tm=ser.readline()
			c = 0
			ser.write('%x' % c)
			c = 1
			while True:
			#for x in range(100):
				tm=ser.readline()
				#print "Send: Got line "+tm;
				v = int(tm[-20:], 16)
				if v != c:
					print "Send: Error!!! "+tm;
					print "RES: ERROR ";
					sys.stdout.flush()
					os._exit(1)
				c = v + 1
				n = '%x' % c;
				if (c==100):
					k= chr(0x54) + chr(0xaa) + '0'*(packet_size-2-len(n));
				else:
					k= chr(0x55) + chr(0xaa) + '0'*(packet_size-2-len(n));
				#time.sleep(1.0001)
				ser.write( k + n )
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

