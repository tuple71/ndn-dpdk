package cstest

import (
	"os"
	"testing"

	"ndn-dpdk/dpdk/dpdktestenv"
	"ndn-dpdk/ndn"
)

func TestMain(m *testing.M) {
	dpdktestenv.MakeDirectMp(1023, ndn.SizeofPacketPriv(), 2000)

	os.Exit(m.Run())
}

var makeAR = dpdktestenv.MakeAR
