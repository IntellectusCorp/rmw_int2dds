Security
========

.. warning::

   **int2DDS does not currently support ROS 2 / DDS-Security.**
   The ``ROS_SECURITY_*`` mechanism (SROS 2) is **not** implemented by
   ``rmw_int2dds_cpp`` at this time. Do not assume any authentication,
   access control, or encryption is in effect when using this RMW.

Current status
--------------

.. list-table::
   :header-rows: 1
   :widths: 45 55

   * - Feature
     - Status
   * - Authentication (DDS:Auth:PKI-DH)
     - Not supported
   * - Access control (DDS:Access:Permissions)
     - Not supported
   * - Cryptographic (DDS:Crypto:AES-GCM-GMAC)
     - Not supported
   * - SROS 2 keystore / ``ROS_SECURITY_*``
     - Not supported

Behavior when security is requested
-----------------------------------

If ``ROS_SECURITY_ENABLE=true`` with ``ROS_SECURITY_STRATEGY=Enforce`` is set,
communication will **not** be secured by this RMW. Plan deployments
accordingly and isolate the network by other means (e.g. a trusted/segmented
network) if confidentiality or authentication is required.

.. note::

   DDS-Security / SROS 2 support is not currently available in int2DDS. This
   page will be updated with setup steps and the supported plugin list if such
   support is added.

Reporting vulnerabilities
-------------------------

To report a security vulnerability, see ``SECURITY.md`` in the repository root.
