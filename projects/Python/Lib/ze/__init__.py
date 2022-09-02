'''
Zeno Python API module
'''


import ctypes
import functools
from typing import Union, Optional, Any, Iterator
from types import MappingProxyType


Numeric = Union[int, float, tuple[int], tuple[float]]
Literial = Union[int, float, tuple[int], tuple[float], str]


def initDLLPath(path: str):
    global api
    api = ctypes.cdll.LoadLibrary(path)

    def chkerr(ret):
        if ret != 0:
            raise RuntimeError('[zeno internal error] {}'.format(ctypes.string_at(api.Zeno_GetLastErrorStr()).decode()))

    def wrapchkerr(func):
        @functools.wraps(func)
        def wrapped(*args):
            chkerr(func(*args))
        return wrapped

    def define(rettype, funcname, *argtypes, do_checks=True):
        func = getattr(api, funcname)
        func.rettype = rettype
        func.argtypes = argtypes
        if do_checks:
            func = wrapchkerr(func)
        setattr(api, funcname, func)

    define(ctypes.c_uint32, 'Zeno_GetLastErrorCode', do_checks=False)
    define(ctypes.c_char_p, 'Zeno_GetLastErrorStr', do_checks=False)
    define(ctypes.c_uint32, 'Zeno_CreateGraph', ctypes.POINTER(ctypes.c_uint64))
    define(ctypes.c_uint32, 'Zeno_DestroyGraph', ctypes.c_uint64)
    define(ctypes.c_uint32, 'Zeno_GraphIncReference', ctypes.c_uint64)
    define(ctypes.c_uint32, 'Zeno_GraphLoadJson', ctypes.c_uint64, ctypes.c_char_p)
    define(ctypes.c_uint32, 'Zeno_GraphCallTempNode', ctypes.c_uint64, ctypes.c_char_p, ctypes.POINTER(ctypes.c_char_p), ctypes.POINTER(ctypes.c_uint64), ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t))
    define(ctypes.c_uint32, 'Zeno_GetLastTempNodeResult', ctypes.POINTER(ctypes.c_char_p), ctypes.POINTER(ctypes.c_uint64))
    define(ctypes.c_uint32, 'Zeno_CreateObjectInt', ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(ctypes.c_int), ctypes.c_size_t)
    define(ctypes.c_uint32, 'Zeno_CreateObjectFloat', ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(ctypes.c_float), ctypes.c_size_t)
    define(ctypes.c_uint32, 'Zeno_CreateObjectString', ctypes.POINTER(ctypes.c_uint64), ctypes.POINTER(ctypes.c_char), ctypes.c_size_t)
    define(ctypes.c_uint32, 'Zeno_CreateObjectPrimitive', ctypes.POINTER(ctypes.c_uint64))
    define(ctypes.c_uint32, 'Zeno_DestroyObject', ctypes.c_uint64)
    define(ctypes.c_uint32, 'Zeno_ObjectIncReference', ctypes.c_uint64)
    define(ctypes.c_uint32, 'Zeno_GetObjectLiterialType', ctypes.c_uint64, ctypes.POINTER(ctypes.c_int))
    define(ctypes.c_uint32, 'Zeno_GetObjectInt', ctypes.c_uint64, ctypes.POINTER(ctypes.c_int), ctypes.c_size_t)
    define(ctypes.c_uint32, 'Zeno_GetObjectFloat', ctypes.c_uint64, ctypes.POINTER(ctypes.c_float), ctypes.c_size_t)
    define(ctypes.c_uint32, 'Zeno_GetObjectString', ctypes.c_uint64, ctypes.POINTER(ctypes.c_char), ctypes.POINTER(ctypes.c_size_t))
    define(ctypes.c_uint32, 'Zeno_GetObjectPrimData', ctypes.c_uint64, ctypes.c_int, ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_int))
    define(ctypes.c_uint32, 'Zeno_GetObjectPrimDataKeys', ctypes.c_uint64, ctypes.c_int, ctypes.POINTER(ctypes.c_size_t), ctypes.POINTER(ctypes.c_char_p))
    define(ctypes.c_uint32, 'Zeno_ResizeObjectPrimData', ctypes.c_uint64, ctypes.c_int, ctypes.c_size_t)


class _MemSpanWrapper:
    _ptr: int
    _len: int
    _type: Any
    _dim: int

    def __init__(self, ptr_: int, len_: int, type_: Any, dim_: int):
        self._ptr = ptr_
        self._len = len_
        self._type = type_
        self._dim = dim_

    def __repr__(self) -> str:
        return '[zeno attribute at {} of len {} with type {} and dim {}]'.format(self._ptr, self._len, self._type, self._dim)

    def __getitem__(self, index: int) -> Numeric:
        if index < 0 or index >= self._len:
            raise IndexError('index {} out of range [0, {})'.format(index, self._len))
        base = ctypes.cast(self._ptr, ctypes.POINTER(self._type))
        return tuple(base[index * self._dim + i] for i in range(self._dim)) if self._dim != 1 else base[index]

    def __setitem__(self, index: int, value: Numeric):
        if index < 0 or index >= self._len:
            raise IndexError('index {} out of range [0, {})'.format(index, self._len))
        base = ctypes.cast(self._ptr, ctypes.POINTER(self._type))
        if self._dim != 1:
            for i in range(self._dim):
                base[index * self._dim + i] = value[i]  # type: ignore
        else:
            base[index] = value

    def to_list(self) -> list[Numeric]:
        base = ctypes.cast(self._ptr, ctypes.POINTER(self._type))
        if self._dim != 1:
            return [tuple(base[index * self._dim + i] for i in range(self._dim)) for index in range(self._len)]
        else:
            return [base[index] for index in range(self._len)]

    def from_list(self, lst: list[Numeric]):
        if len(lst) != self._len:
            raise ValueError('list length mismatch {} != {}', len(lst), self._len)
        base = ctypes.cast(self._ptr, ctypes.POINTER(self._type))
        if self._dim != 1:
            for index, val in enumerate(lst):
                for i in range(self._dim):
                    base[index * self._dim + i] = val[i]  # type: ignore
        else:
            for index, val in enumerate(lst):
                base[index] = val

    def c_data(self) -> tuple[int, int, Any, int]:
        return (self._ptr, self._len, self._type, self._dim)

    def __len__(self) -> int:
        return self._len

    def __iter__(self) -> Iterator[Numeric]:
        return iter(self.to_list())


class _AttrVectorWrapper:
    _handle: int
    _kind: int

    _typeLut = [
        ctypes.c_float,
        ctypes.c_float,
        ctypes.c_int,
        ctypes.c_int,
        ctypes.c_float,
        ctypes.c_int,
        ctypes.c_float,
        ctypes.c_int,
    ]
    _dimLut = [
        3, 1, 3, 1, 2, 2, 4, 4,
    ]

    def __init__(self, handle: int, kind: int):
        self._handle = handle
        self._kind = kind

    def attr(self, attrName: str) -> _MemSpanWrapper:
        ptrRet_ = ctypes.c_void_p()
        lenRet_ = ctypes.c_size_t()
        typeRet_ = ctypes.c_int()
        api.Zeno_GetObjectPrimData(ctypes.c_uint64(self._handle), ctypes.c_int(self._kind), ctypes.c_char_p(attrName.encode()), ctypes.pointer(ptrRet_), ctypes.pointer(lenRet_), ctypes.pointer(typeRet_))
        return _MemSpanWrapper(ptrRet_.value, lenRet_.value, self._typeLut[typeRet_.value], self._dimLut[typeRet_.value])  # type: ignore

    def keys(self) -> list[str]:
        count_ = ctypes.c_size_t(0)
        api.Zeno_GetObjectPrimDataKeys(ctypes.c_uint64(self._handle), ctypes.c_int(self._kind), ctypes.pointer(count_), ctypes.cast(0, ctypes.POINTER(ctypes.c_char_p)))
        keys_ = (ctypes.c_char_p * count_.value)()
        api.Zeno_GetObjectPrimDataKeys(ctypes.c_uint64(self._handle), ctypes.c_int(self._kind), ctypes.pointer(count_), keys_)
        keys: list[str] = list(map(lambda x: x.decode(), keys_))
        return keys

    def values(self) -> list[_MemSpanWrapper]:
        return [self.attr(k) for k in self.keys()]

    def items(self) -> list[tuple[str, _MemSpanWrapper]]:
        return [(k, self.attr(k)) for k in self.keys()]

    def __iter__(self) -> Iterator[tuple[str, _MemSpanWrapper]]:
        return iter(self.items())

    def __repr__(self) -> str:
        return '[zeno attribute collection of primitive at {} of kind {}]'.format(self._handle, self._kind)

    def __contains__(self, key: str) -> bool:
        return key in self.keys()

    def __getitem__(self, key: str) -> _MemSpanWrapper:
        return self.attr(key)

    def __getattr__(self, key: str) -> _MemSpanWrapper:
        return self.attr(key)

    def __len__(self) -> int:
        return self.size()

    def size(self) -> int:
        ptrRet_ = ctypes.c_void_p()
        lenRet_ = ctypes.c_size_t()
        typeRet_ = ctypes.c_int()
        api.Zeno_GetObjectPrimData(ctypes.c_uint64(self._handle), ctypes.c_int(self._kind), ctypes.c_char_p('pos'.encode()), ctypes.pointer(ptrRet_), ctypes.pointer(lenRet_), ctypes.pointer(typeRet_))
        return lenRet_.value

    def resize(self, newSize: int):
        api.Zeno_ResizeObjectPrimData(ctypes.c_uint64(self._handle), ctypes.c_int(self._kind), ctypes.c_size_t(newSize))


class ZenoObject:
    _handle: int

    __create_key = object()

    def __init__(self, create_key: object, handle: int):
        assert create_key is self.__create_key, 'ZenoObject has a private constructor'
        self._handle = handle

    def __del__(self):
        api.Zeno_DestroyObject(ctypes.c_uint64(self._handle))
        self._handle = 0

    def __repr__(self) -> str:
        return '[zeno object at {}]'.format(self._handle)

    @classmethod
    def fromHandle(cls, handle: int):
        api.Zeno_ObjectIncReference(ctypes.c_uint64(handle))
        return cls(cls.__create_key, handle)

    def toHandle(self) -> int:
        return self._handle

    def asPrim(self):
        return ZenoPrimitiveObject.fromHandle(self._handle)

    @classmethod
    def fromLiterial(cls, value: Union[Literial, 'ZenoObject']) -> 'ZenoObject':
        if isinstance(value, ZenoObject):
            return value
        else:
            return cls(cls.__create_key, cls._makeLiterial(value))

    def toLiterial(self) -> Union[Literial, 'ZenoObject']:
        ret = self._fetchLiterial(self._handle)
        if ret is None:
            return self
        else:
            return ret

    @classmethod
    def newPrim(cls):
        return cls(cls.__create_key, cls._makePrimitive())

    @classmethod
    def _makePrimitive(cls) -> int:
        object_ = ctypes.c_uint64(0)
        api.Zeno_CreateObjectPrimitive(ctypes.pointer(object_))
        return object_.value

    @classmethod
    def _makeLiterial(cls, value: Literial) -> int:
        if isinstance(value, int):
            return cls._makeInt(value)
        elif isinstance(value, float):
            return cls._makeFloat(value)
        elif isinstance(value, (tuple, list)):
            if not 1 <= len(value) <= 4:
                raise ValueError('expect 1 <= len(value) <= 4, got {}'.format(len(value)))
            elif not all(isinstance(x, (int, float)) for x in value):
                raise TypeError('expect all elements in value to be int or float')
            elif all(isinstance(x, int) for x in value):
                return cls._makeVecInt(value)  # type: ignore
            else:
                return cls._makeVecFloat(value)
        elif isinstance(value, str):
            return cls._makeString(value)
        else:
            raise TypeError('expect value type to be int, float, tuple of int or float, or str, got {}'.format(type(value)))

    @classmethod
    def _fetchLiterial(cls, handle: int) -> Optional[Literial]:
        litType_ = ctypes.c_int(0)
        api.Zeno_GetObjectLiterialType(ctypes.c_uint64(handle), ctypes.pointer(litType_))
        ty = litType_.value
        if ty == 1:
            return cls._fetchString(handle)
        elif ty == 11:
            return cls._fetchInt(handle)
        elif 12 <= ty <= 14:
            return cls._fetchVecInt(handle, ty - 10)
        elif ty == 21:
            return cls._fetchFloat(handle)
        elif 22 <= ty <= 24:
            return cls._fetchVecFloat(handle, ty - 20)
        else:
            return None

    @staticmethod
    def _makeInt(value: int) -> int:
        object_ = ctypes.c_uint64(0)
        value_ = ctypes.c_int(value)
        api.Zeno_CreateObjectInt(ctypes.pointer(object_), ctypes.pointer(value_), ctypes.c_size_t(1))
        return object_.value

    @staticmethod
    def _makeFloat(value: float) -> int:
        object_ = ctypes.c_uint64(0)
        value_ = ctypes.c_float(value)
        api.Zeno_CreateObjectFloat(ctypes.pointer(object_), ctypes.pointer(value_), ctypes.c_size_t(1))
        return object_.value

    @staticmethod
    def _makeVecInt(value: tuple[int]) -> int:
        n = len(value)
        assert 1 <= n <= 4
        object_ = ctypes.c_uint64(0)
        value_ = (ctypes.c_int * n)(*value)
        api.Zeno_CreateObjectInt(ctypes.pointer(object_), value_, ctypes.c_size_t(n))
        return object_.value

    @staticmethod
    def _makeVecFloat(value: tuple[float]) -> int:
        n = len(value)
        assert 1 <= n <= 4
        object_ = ctypes.c_uint64(0)
        value_ = (ctypes.c_float * n)(*value)
        api.Zeno_CreateObjectFloat(ctypes.pointer(object_), value_, ctypes.c_size_t(n))
        return object_.value

    @staticmethod
    def _makeString(value: str) -> int:
        arr = value.encode()
        n = len(arr)
        object_ = ctypes.c_uint64(0)
        value_ = (ctypes.c_char * n)(*arr)
        api.Zeno_CreateObjectString(ctypes.pointer(object_), value_, ctypes.c_size_t(n))
        return object_.value

    @staticmethod
    def _fetchInt(handle: int) -> int:
        value_ = ctypes.c_int(0)
        api.Zeno_GetObjectInt(ctypes.c_uint64(handle), ctypes.pointer(value_), ctypes.c_size_t(1))
        return value_.value

    @staticmethod
    def _fetchFloat(handle: int) -> float:
        value_ = ctypes.c_float(0)
        api.Zeno_GetObjectFloat(ctypes.c_uint64(handle), ctypes.pointer(value_), ctypes.c_size_t(1))
        return value_.value

    @staticmethod
    def _fetchVecInt(handle: int, dim: int) -> tuple[int]:
        assert 1 <= dim <= 4
        value_ = (ctypes.c_int * dim)()
        api.Zeno_GetObjectInt(ctypes.c_uint64(handle), value_, ctypes.c_size_t(dim))
        return tuple(value_)

    @staticmethod
    def _fetchVecFloat(handle: int, dim: int) -> tuple[float]:
        assert 1 <= dim <= 4
        value_ = (ctypes.c_float * dim)()
        api.Zeno_GetObjectFloat(ctypes.c_uint64(handle), value_, ctypes.c_size_t(dim))
        return tuple(value_)

    @staticmethod
    def _fetchString(handle: int) -> str:
        strLen_ = ctypes.c_size_t(0)
        api.Zeno_CreateObjectString(ctypes.c_uint64(handle), ctypes.c_void_p(0), ctypes.pointer(strLen_))
        value_ = (ctypes.c_char * strLen_.value)()
        api.Zeno_CreateObjectString(ctypes.c_uint64(handle), value_, ctypes.pointer(strLen_))
        return bytes(value_).decode()


class ZenoPrimitiveObject(ZenoObject):
    def _getArray(self, kind: int) -> _AttrVectorWrapper:
        return _AttrVectorWrapper(self._handle, kind)

    def __repr__(self) -> str:
        return '[zeno primitive at {}]'.format(self._handle)

    @classmethod
    def new(cls):
        return cls.newPrim()

    @property
    def verts(self):
        return self._getArray(0)

    @property
    def points(self):
        return self._getArray(1)

    @property
    def lines(self):
        return self._getArray(2)

    @property
    def tris(self):
        return self._getArray(3)

    @property
    def quads(self):
        return self._getArray(4)

    @property
    def polys(self):
        return self._getArray(5)

    @property
    def loops(self):
        return self._getArray(6)

    @property
    def uvs(self):
        return self._getArray(7)

    @property
    def loop_uvs(self):
        return self._getArray(8)

    def asObject(self):
        return ZenoObject.fromHandle(self._handle)


class ZenoGraph:
    _handle: int

    __create_key = object()

    def __init__(self, create_key: object, handle: int):
        assert create_key is self.__create_key, 'ZenoGraph has a private constructor'
        self._handle = handle

    def __repr__(self) -> str:
        return '[zeno graph at {}]'.format(self._handle)

    @classmethod
    def new(cls):
        graph_ = ctypes.c_uint64(0)
        api.Zeno_CreateGraph(ctypes.pointer(graph_))
        return cls(cls.__create_key, graph_.value)

    @classmethod
    def current(cls):
        handle = _currgraph
        if handle == 0:
            raise RuntimeError('no current graph')
        api.Zeno_GraphIncReference(ctypes.c_uint64(handle))
        return cls(cls.__create_key, handle)

    def callTempNode(self, nodeType: str, inputs: dict[str, int]) -> dict[str, int]:
        inputCount_ = len(inputs)
        inputKeys_ = (ctypes.c_char_p * inputCount_)(*map(lambda x: x.encode(), inputs.keys()))
        inputObjects_ = (ctypes.c_uint64 * inputCount_)(*inputs.values())
        outputCount_ = ctypes.c_size_t(0)
        api.Zeno_GraphCallTempNode(ctypes.c_uint64(self._handle), ctypes.c_char_p(nodeType.encode()), inputKeys_, inputObjects_, ctypes.c_size_t(inputCount_), ctypes.pointer(outputCount_))
        outputKeys_ = (ctypes.c_char_p * outputCount_.value)()
        outputObjects_ = (ctypes.c_uint64 * outputCount_.value)()
        api.Zeno_GetLastTempNodeResult(outputKeys_, outputObjects_)
        outputs: dict[str, int] = dict(zip(map(lambda x: x.decode(), outputKeys_), outputObjects_))
        return outputs

    def __del__(self):
        api.Zeno_DestroyGraph(ctypes.c_uint64(self._handle))
        self._handle = 0


_args : dict[str, int] = {}
_rets : dict[str, int] = {}
_currgraph : int = 0


def has_input(key: str) -> bool:
    return key in _args


def get_input(key: str) -> ZenoObject:
    if key not in _args:
        raise KeyError('invalid input key: {}'.format(key))
    return ZenoObject.fromHandle(_args[key])


def set_output(key: str, value: ZenoObject):
    _rets[key] = ZenoObject.toHandle(value)


def get_input2(key: str) -> Union[Literial, 'ZenoObject']:
    return ZenoObject.toLiterial(get_input(key))


def set_output2(key: str, value: Union[Literial, 'ZenoObject']):
    set_output(key, ZenoObject.fromLiterial(value))


class _TempNodeWrapper:
    class _MappingProxyWrapper:
        _prox: MappingProxyType

        def __init__(self, lut: dict[str, Union[Literial, ZenoObject]]):
            self._prox = MappingProxyType(lut)

        def __getattr__(self, key: str) -> Union[Literial, ZenoObject]:
            return self._prox[key]

        def __getitem__(self, key: str) -> Union[Literial, ZenoObject]:
            return self._prox[key]

    def __getattr__(self, key: str):
        def wrapped(**args: dict[str, Union[Literial, ZenoObject]]):
            currGraph = ZenoGraph.current()
            store_args : dict[str, ZenoObject] = {k: ZenoObject.fromLiterial(v) for k, v in args.items()}  # type: ignore
            inputs : dict[str, int] = {k: ZenoObject.toHandle(v) for k, v in store_args.items()}
            outputs : dict[str, int] = currGraph.callTempNode(key, inputs)
            rets : dict[str, Union[Literial, ZenoObject]] = {k: ZenoObject.toLiterial(ZenoObject.fromHandle(v)) for k, v in outputs.items()}
            return self._MappingProxyWrapper(rets)
        wrapped.__name__ = key
        wrapped.__qualname__ = __name__ + '.no.' + key
        setattr(self, key, wrapped)
        return wrapped

no = _TempNodeWrapper()


__all__ = [
        'ZenoGraph',
        'ZenoObject',
        'has_input',
        'get_input',
        'set_output',
        'get_input2',
        'set_output2',
        'no',
        ]
