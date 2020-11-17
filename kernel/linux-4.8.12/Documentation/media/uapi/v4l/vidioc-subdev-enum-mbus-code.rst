.. -*- coding: utf-8; mode: rst -*-

.. _VIDIOC_SUBDEV_ENUM_MBUS_CODE:

**********************************
ioctl VIDIOC_SUBDEV_ENUM_MBUS_CODE
**********************************

Name
====

VIDIOC_SUBDEV_ENUM_MBUS_CODE - Enumerate media bus formats


Synopsis
========

.. cpp:function:: int ioctl( int fd, int request, struct v4l2_subdev_mbus_code_enum * argp )


Arguments
=========

``fd``
    File descriptor returned by :ref:`open() <func-open>`.

``request``
    VIDIOC_SUBDEV_ENUM_MBUS_CODE

``argp``


Description
===========

To enumerate media bus formats available at a given sub-device pad
applications initialize the ``pad``, ``which`` and ``index`` fields of
struct
:ref:`v4l2_subdev_mbus_code_enum <v4l2-subdev-mbus-code-enum>` and
call the :ref:`VIDIOC_SUBDEV_ENUM_MBUS_CODE` ioctl with a pointer to this
structure. Drivers fill the rest of the structure or return an ``EINVAL``
error code if either the ``pad`` or ``index`` are invalid. All media bus
formats are enumerable by beginning at index zero and incrementing by
one until ``EINVAL`` is returned.

Available media bus formats may depend on the current 'try' formats at
other pads of the sub-device, as well as on the current active links.
See :ref:`VIDIOC_SUBDEV_G_FMT` for more
information about the try formats.


.. _v4l2-subdev-mbus-code-enum:

.. flat-table:: struct v4l2_subdev_mbus_code_enum
    :header-rows:  0
    :stub-columns: 0
    :widths:       1 1 2


    -  .. row 1

       -  __u32

       -  ``pad``

       -  Pad number as reported by the media controller API.

    -  .. row 2

       -  __u32

       -  ``index``

       -  Number of the format in the enumeration, set by the application.

    -  .. row 3

       -  __u32

       -  ``code``

       -  The media bus format code, as defined in
	  :ref:`v4l2-mbus-format`.

    -  .. row 4

       -  __u32

       -  ``which``

       -  Media bus format codes to be enumerated, from enum
	  :ref:`v4l2_subdev_format_whence <v4l2-subdev-format-whence>`.

    -  .. row 5

       -  __u32

       -  ``reserved``\ [8]

       -  Reserved for future extensions. Applications and drivers must set
	  the array to zero.


Return Value
============

On success 0 is returned, on error -1 and the ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.

EINVAL
    The struct
    :ref:`v4l2_subdev_mbus_code_enum <v4l2-subdev-mbus-code-enum>`
    ``pad`` references a non-existing pad, or the ``index`` field is out
    of bounds.
