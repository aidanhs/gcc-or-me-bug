#define _LARGEFILE64_SOURCE 1
#define PY_SSIZE_T_CLEAN 1
#undef NDEBUG
#include "../../config/config.h"

// According to Python, its header has to go first:
//   http://docs.python.org/2/c-api/intro.html#include-files
#include <Python.h>

#include "bupsplit.h"

#define BLOB_READ_SIZE 1024*1024
#define BLOB_MAX 8192*4

typedef struct {
    PyObject_HEAD
    ssize_t ofs;
    Py_ssize_t filenum;
    /* Make sure python doesn't garbage collect and close the files */
    PyObject *curfile;
    int fd;
    PyObject *fileiter;
    PyObject *progressfn;
    Py_ssize_t prevread;
    char buf[BLOB_READ_SIZE];
} readfile_iter_state;

// Return -1 if the next file cannot be obtained (end of iter or error)
static int next_file(readfile_iter_state *s)
{
    PyObject *fdobj;
    Py_XDECREF(s->curfile);
    s->curfile = PyIter_Next(s->fileiter);
    if (s->curfile == NULL)
        return -1;
    if (PyObject_HasAttrString(s->curfile, "fileno")) {
        fdobj = PyObject_CallMethod(s->curfile, "fileno", NULL);
        if (fdobj == NULL)
            return -1;
        s->fd = (int)PyInt_AsLong(fdobj);
        Py_DECREF(fdobj);
        /* fd can never be -1 */
        if (s->fd == -1) {
            if (PyErr_Occurred() == NULL)
                PyErr_SetString(PyExc_ValueError, "invalid file descriptor");
            return -1;
        }
    } else {
        s->fd = -1;
    }
    return 0;
}

static void fadvise_done(int fd, ssize_t ofs)
{
#ifdef POSIX_FADV_DONTNEED
    posix_fadvise(fd, 0, ofs, POSIX_FADV_DONTNEED);
#endif
}

/* Note we don't report progress when looping as we won't have read any bytes */
static PyObject* readfile_iter_iternext(PyObject *self)
{
    readfile_iter_state *s;
    ssize_t bytes_read;
    PyObject *bytesobj = NULL;

    s = (readfile_iter_state *)self;
    if (s->progressfn != NULL)
        PyObject_CallFunction(s->progressfn, "nn", s->filenum, s->prevread);
    if (s->ofs > 1024*1024)
        fadvise_done(s->fd, s->ofs - 1024*1024);

    while (1) {
        int realfile = (s->fd != -1);
        if (realfile) {
            bytes_read = read(s->fd, s->buf, BLOB_READ_SIZE);
        } else { /* Not a real file TODO: use PyObject_CallMethodObjArgs */
            bytesobj = PyObject_CallMethod(
                s->curfile, "read", "n", BLOB_READ_SIZE, NULL);
            if (bytesobj == NULL)
                return NULL;
            bytes_read = PyString_GET_SIZE(bytesobj);
        }
        if (bytes_read > 0) {
            s->prevread = bytes_read;
            s->ofs += bytes_read;
            if (realfile) {
                return Py_BuildValue("s#", s->buf, bytes_read);
            } else {
                return bytesobj;
            }
        } else if (bytes_read == 0) {
            if (realfile)
                fadvise_done(s->fd, s->ofs);
            if (next_file(s) != -1) { /* End of file */
                s->ofs = 0;
                s->filenum++;
                /* Don't recurse to avoid stack overflow, loop instead */
            } else {
                if (PyErr_Occurred() == NULL)
                    PyErr_SetNone(PyExc_StopIteration);
                return NULL;
            }
        } else {
            PyErr_SetString(PyExc_IOError, "failed to read file");
            return NULL;
        }
    }
}

static PyObject *
readfile_iter_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    PyObject *iterarg;
    PyObject *progressfn = NULL;

    char *kw[] = {"files", "progress", NULL};
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "O|O:readfile_iter", kw, &iterarg, &progressfn))
        return NULL;

    /* We expect an argument that supports the iterator protocol, but we might
     * need to convert it from e.g. a list first
     */
    PyObject *fileiter = PyObject_GetIter(iterarg);
    if (fileiter == NULL)
        return NULL;
    if (!PyIter_Check(fileiter)) {
        PyErr_SetString(PyExc_TypeError,
            "readfile_iter() expects an iterator of files");
        return NULL;
    }

    if (progressfn == Py_None) {
        Py_DECREF(progressfn);
        progressfn = NULL;
    }
    if (progressfn != NULL && !PyCallable_Check(progressfn)) {
        PyErr_SetString(PyExc_TypeError,
            "readfile_iter() expects a callable progress function");
        return NULL;
    }

    readfile_iter_state *state =
        (readfile_iter_state *)type->tp_alloc(type, 0);
    if (!state)
        return NULL;

    state->fileiter = fileiter;
    state->ofs = 0;
    state->filenum = 0;
    state->progressfn = progressfn;
    state->prevread = 0;
    state->curfile = NULL;
    /* Will initialise fd and curfile with the first file */
    if (next_file(state) == -1)
        return NULL;

    Py_INCREF(fileiter);
    Py_XINCREF(progressfn);

    return (PyObject *)state;
}

static void
readfile_iter_dealloc(PyObject *iterstate)
{
    readfile_iter_state *state = (readfile_iter_state *)iterstate;
    Py_DECREF(state->fileiter);
    Py_XDECREF(state->curfile);
    Py_XDECREF(state->progressfn);
    Py_TYPE(iterstate)->tp_free(iterstate);
}

static PyTypeObject readfile_iter = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "readfile_iter",           /* tp_name */
    sizeof(readfile_iter_state), /* tp_basicsize */
    0,                         /* tp_itemsize */
    readfile_iter_dealloc,     /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,        /* tp_flags */
    0,                         /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    PyObject_SelfIter,         /* tp_iter */
    readfile_iter_iternext,    /* tp_iternext */
    0,                         /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    PyType_GenericAlloc,       /* tp_alloc */
    readfile_iter_new          /* tp_new */
};

/********************************************************/
/********************************************************/

typedef struct {
    PyObject_HEAD
    PyObject *bufobj;
    int basebits;
    int fanbits;
    unsigned char prevbuf[BLOB_MAX];
} splitbuf_state;

static int splitbuf_actual(
    const unsigned char *buf, Py_ssize_t len, int *ofsptr, int *bitsptr)
{
    int ofs = 0, bits = -1;

    assert(len <= INT_MAX);
    ofs = bupsplit_find_ofs(buf, len, &bits);
    if (ofs)
        assert(bits >= BUP_BLOBBITS);
    *ofsptr = ofs;
    *bitsptr = bits;
    return 0;
}

static PyObject* splitbuf_iternext(PyObject *self)
{
    splitbuf_state *s = (splitbuf_state *)self;
    PyObject *bufused;
    PyObject *bufpeekobj;
    const unsigned char *bufpeekbytes;
    Py_ssize_t bufpeeklen;
    int ofs, bits;
    int level;
//
    bufused = PyObject_CallMethod(s->bufobj, "used", NULL);
    if (bufused == NULL)
        return NULL;
    bufpeekobj = PyObject_CallMethod(s->bufobj, "peek", "O", bufused);
    Py_DECREF(bufused);
    printf("test %p\n", bufpeekbytes);
    if (bufpeekbytes == NULL)
        return NULL;
    if (PyObject_AsCharBuffer(
            bufpeekobj, ((const char **)&bufpeekbytes), &bufpeeklen) == -1)
        return NULL;
    if (splitbuf_actual(bufpeekbytes, bufpeeklen, &ofs, &bits) == -1)
        return NULL;
    Py_DECREF(bufpeekbytes);
    if (ofs > BLOB_MAX)
        ofs = BLOB_MAX;
    printf("ofs %d\n", ofs);
    if (ofs) {
        PyObject *tmp = PyObject_CallMethod(s->bufobj, "eat", "i", ofs);
        if (tmp == NULL)
            return NULL;
        Py_DECREF(tmp);
        level = (bits - s->basebits) / s->fanbits;
        printf("%d\n", bits);
        memcpy(s->prevbuf, bufpeekbytes, ofs);
        PyObject *retbuf = PyBuffer_FromMemory(s->prevbuf, ofs);
        if (retbuf == NULL)
            return NULL;
        printf("zzz\n");
        return Py_BuildValue("Ni", retbuf, level);
    }

    return NULL;
}

static PyObject *
splitbuf_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    PyObject *bufobj;
    int basebits;
    int fanbits;

    if (!PyArg_ParseTuple(args, "Oii:_splitbuf", &bufobj, &basebits, &fanbits))
        return NULL;

    /* TODO: don't assume this is a buf object */

    splitbuf_state *state = (splitbuf_state *)type->tp_alloc(type, 0);
    if (!state)
        return NULL;

    Py_INCREF(bufobj);
    state->bufobj = bufobj;
    state->basebits = basebits;
    state->fanbits = fanbits;

    return (PyObject *)state;
}

static void
splitbuf_dealloc(PyObject *iterstate)
{
    splitbuf_state *state = (splitbuf_state *)iterstate;
    Py_DECREF(state->bufobj);
    Py_TYPE(iterstate)->tp_free(iterstate);
}

static PyTypeObject splitbuf = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size */
    "_splitbuf",               /* tp_name */
    sizeof(splitbuf_state),    /* tp_basicsize */
    0,                         /* tp_itemsize */
    splitbuf_dealloc,          /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_compare */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,        /* tp_flags */
    0,                         /* tp_doc */
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    PyObject_SelfIter,         /* tp_iter */
    splitbuf_iternext,         /* tp_iternext */
    0,                         /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    0,                         /* tp_init */
    PyType_GenericAlloc,       /* tp_alloc */
    splitbuf_new               /* tp_new */
};

/********************************************************/
/********************************************************/

static PyMethodDef hashsplit_methods[] = {
    { NULL, NULL, 0, NULL },  // sentinel
};

PyMODINIT_FUNC init_hashsplit(void)
{
    PyObject *m = Py_InitModule("_hashsplit", hashsplit_methods);
    if (m == NULL)
        return;

    if (PyType_Ready(&readfile_iter) < 0)
        return;
    if (PyType_Ready(&splitbuf) < 0)
        return;
    Py_INCREF((PyObject *)&readfile_iter);
    Py_INCREF((PyObject *)&splitbuf);
    PyModule_AddObject(m, "readfile_iter", (PyObject *)&readfile_iter);
    PyModule_AddObject(m, "_splitbuf", (PyObject *)&splitbuf);
}
