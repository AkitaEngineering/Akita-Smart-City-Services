#ifndef ACTUATOR_INTERFACE_H
#define ACTUATOR_INTERFACE_H

#include <string>

class ActuatorInterface {
  public:
    virtual ~ActuatorInterface() = default;
    virtual std::string getAssetId() const = 0;
    virtual bool execute(const std::string &action, bool isNumeric, float numericValue, bool booleanValue,
                         std::string &detail) = 0;
};

#endif
