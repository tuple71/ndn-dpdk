package iface

/*
#include "face.h"

void go_Face_RxCb(Face* face, FaceRxBurst* burst, void* cbarg);
*/
import "C"
import (
	"sync"
	"unsafe"
)

type RxCbFunc func(face Face, burst RxBurst)

var rxCbFuncs = make([]RxCbFunc, 0)
var rxCbFuncsLock sync.RWMutex

// Wrap a Go RxCbFunc into *C.Face_RxCb and cbarg.
func WrapRxCb(f RxCbFunc) (cb unsafe.Pointer, cbarg unsafe.Pointer) {
	rxCbFuncsLock.Lock()
	defer rxCbFuncsLock.Unlock()
	index := len(rxCbFuncs)
	rxCbFuncs = append(rxCbFuncs, f)
	return unsafe.Pointer(C.go_Face_RxCb), unsafe.Pointer(uintptr(index))
}

//export go_Face_RxCb
func go_Face_RxCb(face *C.Face, burst *C.FaceRxBurst, cbarg unsafe.Pointer) {
	index := uintptr(cbarg)
	rxCbFuncsLock.RLock()
	f := rxCbFuncs[index]
	rxCbFuncsLock.RUnlock()

	f(Face{face}, RxBurst{burst})
}

// Interface containing RxLoop and related functions.
type IRxLooper interface {
	RxLoop(burstSize int, cb unsafe.Pointer, cbarg unsafe.Pointer)
	StopRxLoop() error
	ListFacesInRxLoop() []FaceId
}