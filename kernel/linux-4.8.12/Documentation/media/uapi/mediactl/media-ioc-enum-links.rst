.. -*- coding: utf-8; mode: rst -*-

.. _media_ioc_enum_links:

**************************
ioctl MEDIA_IOC_ENUM_LINKS
**************************

Name
====

MEDIA_IOC_ENUM_LINKS - Enumerate all pads and links for a given entity


Synopsis
========

.. cpp:function:: int ioctl( int fd, int request, struct media_links_enum *argp )


Arguments
=========

``fd``
    File descriptor returned by :ref:`open() <media-func-open>`.

``request``
    MEDIA_IOC_ENUM_LINKS

``argp``


Description
===========

To enumerate pads and/or links for a given entity, applications set the
entity field of a struct :ref:`media_links_enum <media-links-enum>`
structure and initialize the struct
:ref:`media_pad_desc <media-pad-desc>` and struct
:ref:`media_link_desc <media-link-desc>` structure arrays pointed by
the ``pads`` and ``links`` fields. They then call the
MEDIA_IOC_ENUM_LINKS ioctl with a pointer to this structure.

If the ``pads`` field is not NULL, the driver fills the ``pads`` array
with information about the entity's pads. The array must have enough
room to store all the entity's pads. The number of pads can be retrieved
with :ref:`MEDIA_IOC_ENUM_ENTITIES`.

If the ``links`` field is not NULL, the driver fills the ``links`` array
with information about the entity's outbound links. The array must have
enough room to store all the entity's outbound links. The number of
outbound links can be retrieved with :ref:`MEDIA_IOC_ENUM_ENTITIES`.

Only forward links that originate at one of the entity's source pads are
returned during the enumeration process.


.. _media-links-enum:

.. flat-table:: struct media_links_enum
    :header-rows:  0
    :stub-columns: 0
    :widths:       1 1 2


    -  .. row 1

       -  __u32

       -  ``entity``

       -  Entity id, set by the application.

    -  .. row 2

       -  struct :ref:`media_pad_desc <media-pad-desc>`

       -  \*\ ``pads``

       -  Pointer to a pads array allocated by the application. Ignored if
	  NULL.

    -  .. row 3

       -  struct :ref:`media_link_desc <media-link-desc>`

       -  \*\ ``links``

       -  Pointer to a links array allocated by the application. Ignored if
	  NULL.



.. _media-pad-desc:

.. flat-table:: struct media_pad_desc
    :header-rows:  0
    :stub-columns: 0
    :widths:       1 1 2


    -  .. row 1

       -  __u32

       -  ``entity``

       -  ID of the entity this pad belongs to.

    -  .. row 2

       -  __u16

       -  ``index``

       -  0-based pad index.

    -  .. row 3

       -  __u32

       -  ``flags``

       -  Pad flags, see :ref:`media-pad-flag` for more details.



.. _media-link-desc:

.. flat-table:: struct media_link_desc
    :header-rows:  0
    :stub-columns: 0
    :widths:       1 1 2


    -  .. row 1

       -  struct :ref:`media_pad_desc <media-pad-desc>`

       -  ``source``

       -  Pad at the origin of this link.

    -  .. row 2

       -  struct :ref:`media_pad_desc <media-pad-desc>`

       -  ``sink``

       -  Pad at the target of this link.

    -  .. row 3

       -  __u32

       -  ``flags``

       -  Link flags, see :ref:`media-link-flag` for more details.


Return Value
============

On success 0 is returned, on error -1 and the ``errno`` variable is set
appropriately. The generic error codes are described at the
:ref:`Generic Error Codes <gen-errors>` chapter.

EINVAL
    The struct :ref:`media_links_enum <media-links-enum>` ``id``
    references a non-existing entity.
