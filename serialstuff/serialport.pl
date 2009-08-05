#!/usr/bin/perl -w
use strict;
use Device::SerialPort;
use IO::Socket;

sub sendpaket ($)
{
	my $temperatur = shift;
	my $target = "10.1.1.5";

	my $remote = IO::Socket::INET->new ( Proto => "udp", PeerAddr => $target, PeerPort => "2000");

	print $remote $temperatur;
}

my $serial = Device::SerialPort->new(  "/dev/ttyS0");
$serial->baudrate(1200);
$serial->databits(8);
$serial->purge_all();
$serial->rts_active(0);
$serial->dtr_active(1);
 # Send request
 # Wait one second
select(undef, undef, undef, 1);
 # Read response
my $dline;
my($count, $data);
my $i = 0;
do {
	$data = $serial->read(1);
	$dline .= $data;
	if ($data eq "\n") 
	{
		$i++;
		$dline =~ m /Temperatur:  (-?\d+) Celcius/ ;
		print $1 . "\n";
		sendpaket($1);
		$dline = "";
	}
}
while ($i < 3)
