#define _LARGEFILE64_SOURCE 1
#define PY_SSIZE_T_CLEAN 1
#undef NDEBUG

// According to Python, its header has to go first:
//   http://docs.python.org/2/c-api/intro.html#include-files
#include <Python.h>

#include "bupsplit.h"

#define BLOB_READ_SIZE 1024*1024
#define BLOB_MAX 8192*4

/********************************************************/
/********************************************************/

typedef struct {
    PyObject_HEAD
    PyObject *bufobj;
    int basebits;
    int fanbits;
    unsigned char prevbuf[BLOB_MAX];
} splitbuf_state;

static PyObject* splitbuf_iternext(PyObject *self)
{
    splitbuf_state *s = (splitbuf_state *)self;
    PyObject *bufused;
    PyObject *bufpeekobj;
    const unsigned char *bufpeekbytes;
    Py_ssize_t bufpeeklen;
    int ofs, bits;
    int level;

    printf("ITERATION\n");
    bufused = PyObject_CallMethod(s->bufobj, "used", NULL);
    if (bufused == NULL)
        return NULL;
    bufpeekobj = PyObject_CallMethod(s->bufobj, "peek", "O", bufused);
    Py_DECREF(bufused);
    if (bufpeekbytes == NULL)
        return NULL;
    if (PyObject_AsCharBuffer(
            bufpeekobj, ((const char **)&bufpeekbytes), &bufpeeklen) == -1)
        return NULL;

    int bits_ = -1;
    ofs = bupsplit_find_ofs(bufpeekbytes, bufpeeklen, &bits_);
    bits = bits_;

    Py_DECREF(bufpeekbytes);
    if (ofs) {
        PyObject *tmp = PyObject_CallMethod(s->bufobj, "eat", "i", ofs);
        if (tmp == NULL)
            return NULL;
        Py_DECREF(tmp);
        level = (bits - s->basebits) / s->fanbits;
        // THIS LINE BELOW TRIGGERS BUG
        //printf("(ignore me) %d\n", bits);
        memcpy(s->prevbuf, bufpeekbytes, ofs);
        return Py_BuildValue("i", level);
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

    if (PyType_Ready(&splitbuf) < 0)
        return;
    Py_INCREF((PyObject *)&splitbuf);
    PyModule_AddObject(m, "_splitbuf", (PyObject *)&splitbuf);
}
