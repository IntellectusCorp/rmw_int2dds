Architecture (Entity Mapping)
=============================

This page explains how ROS 2 entities map onto int2DDS (DDS) entities inside
``rmw_int2dds_cpp``. It helps users and reviewers understand the structural
relationship between the two object models.

Overview
--------

.. code-block:: text

   ROS 2 (rclcpp / rclpy)          rmw_int2dds_cpp              int2DDS (DDS)
   ─────────────────────          ────────────────             ─────────────
   Context (rclcpp::init)  ────►  rmw_context_t        ────►   DomainParticipant
   Node                    ────►  rmw_node_t           ────►   (shares the participant)
   Publisher               ────►  rmw_publisher_t      ────►   Publisher + DataWriter
   Subscription            ────►  rmw_subscription_t   ────►   Subscriber + DataReader
   Service (server)        ────►  rmw_service_t        ────►   DataReader(rq/) + DataWriter(rr/)
   Client                  ────►  rmw_client_t         ────►   DataWriter(rq/) + DataReader(rr/)
   Wait set / Guard cond.  ────►  rmw_wait_set_t       ────►   listeners / conditions

Domain participant
------------------

Since ROS 2 Foxy, one DDS ``DomainParticipant`` is created **per context**
(typically one per process), not per node. All nodes in a process share that
participant. The ROS domain is selected with ``ROS_DOMAIN_ID``, which maps
directly to the DDS domain id.

Node and graph discovery
------------------------

Because nodes do not own a participant, node-level graph information
(which nodes exist, with which publishers/subscriptions) is shared through the
``ros_discovery_info`` topic using the ``rmw_dds_common`` package's
``ParticipantEntitiesInfo`` message, and cached in a common graph cache.

Topic and type name mapping
---------------------------

ROS 2 names are mangled onto DDS topics with reserved prefixes:

=====================  ============================================
ROS 2                  DDS topic name
=====================  ============================================
Topic ``/chatter``     ``rt/chatter``
Service request        ``rq/<service name>Request``
Service reply          ``rr/<service name>Reply``
=====================  ============================================

Message types follow the DDS type naming convention
``<package>::msg::dds_::<Type>_`` (e.g. ``std_msgs::msg::dds_::String_``).

Publishers and subscriptions
----------------------------

``rmw_create_publisher()`` creates an int2DDS ``Publisher`` and ``DataWriter``
pair for the mangled topic; ``rmw_create_subscription()`` creates a
``Subscriber`` and ``DataReader``. Type support handles (introspection C/C++)
are used to serialize ROS messages to XCDR for the wire.

Services and clients
--------------------

A service server is a (``rq/`` reader, ``rr/`` writer) pair; a client is the
mirrored (``rq/`` writer, ``rr/`` reader) pair. Requests and responses are
correlated with a GUID + sequence-number pair carried per RPC sample.

QoS and security
----------------

QoS mapping is documented in :doc:`qos_mapping`; security (sros2) integration
is documented in :doc:`security`.
