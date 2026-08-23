# Supported deployment patterns

ASCS can carry bounded numeric sensor telemetry for environmental monitoring, utility/asset state, and fleet coordinates when a corresponding physical sensor implementation supplies those readings. The console recognizes environmental, fleet, infrastructure, and diagnostic reading keys documented by its typed telemetry model.

ASCS can also control municipal assets when a concrete actuator implementation validates each supported action and provides a local safe state. Examples include lighting or pump enable/disable, but their use requires a site-specific hazard analysis, trusted-gateway provisioning, broker/Firebase authorization, and physical acceptance testing.

The LoRa mesh can continue forwarding local packets during an IP outage, and the gateway queues telemetry while MQTT is unavailable. The web console and remote operator path still depend on gateway IP, MQTT, and Firebase connectivity; ASCS is not an emergency-services communications substitute.
