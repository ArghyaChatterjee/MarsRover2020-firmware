#include "ClawController.h"

ClawController::ClawController(t_actuatorConfig actuatorConfig, 
                       Motor &motor, Encoder &encoder,  
                       DigitalIn &limSwitchMax, FSR &forceSensor, Servo &tooltipServo,
                       float tooltipExtendedAngle_Degrees, float tooltipRetractedAngle_Degrees, float calibrationTimeout_Seconds, float maxForceFSR_Newtons) : 
        ActuatorController(actuatorConfig, motor, encoder, NULL_DIGITAL_IN, limSwitchMax), r_forceSensor(forceSensor), r_tooltipServo(tooltipServo),
        m_tooltipExtendedAngle_Degrees(tooltipExtendedAngle_Degrees), m_tooltipRetractedAngle_Degrees(tooltipRetractedAngle_Degrees),
        m_calibrationTimeout_Seconds(calibrationTimeout_Seconds), m_max_force_fsr_newtons(maxForceFSR_Newtons){};

mbed_error_status_t ClawController::setMotorPower_Percentage(float percentage) {

    mbed_error_status_t err_status = MBED_ERROR_INVALID_OPERATION;

    if (m_mutex.trylock_for(200)) {
        err_status = ActuatorController::setMotorPower_Percentage(percentage);
    }
    else {
        return MBED_ERROR_MUTEX_LOCK_FAILED;
    }

    m_mutex.unlock();
    return err_status;
}

mbed_error_status_t ClawController::setGapVelocity_CmPerSec(float cmPerSec) {
    
    float shaftVelocity_DegreesPerSec = convertGapVelocityCmToShaftVelocityDegrees(cmPerSec);
    mbed_error_status_t err_status = MBED_ERROR_INVALID_OPERATION;

    if (m_mutex.trylock_for(200)) {
        err_status = setVelocity_DegreesPerSec(shaftVelocity_DegreesPerSec);
    }
    else {
        return MBED_ERROR_MUTEX_LOCK_FAILED;
    }

    m_mutex.unlock();
    return err_status;
}

mbed_error_status_t ClawController::setGapDistance_Cm(float cm) {

    float shaftPosition_Degrees = convertGapCmToShaftPositionDegrees(cm);
    mbed_error_status_t err_status = MBED_ERROR_INVALID_OPERATION;

    if (m_mutex.trylock_for(200)) { 
        err_status = setAngle_Degrees(shaftPosition_Degrees);
    }
    else {
        return MBED_ERROR_MUTEX_LOCK_FAILED;
    }

    m_mutex.unlock();
    return err_status;
}

mbed_error_status_t ClawController::setMotionData(float motionData) {
    switch(getControlMode()) {
        case t_actuatorControlMode::motorPower:
            return setMotorPower_Percentage(motionData);
        case t_actuatorControlMode::velocity:
            return setGapVelocity_CmPerSec(motionData);
        case t_actuatorControlMode::position:
            return setGapDistance_Cm(motionData);
        default: 
            return MBED_ERROR_INVALID_ARGUMENT;
    }
}

mbed_error_status_t ClawController::extendToolTip() {
    mbed_error_status_t err_status = r_tooltipServo.set_position(m_tooltipExtendedAngle_Degrees) ? MBED_SUCCESS : MBED_ERROR_INVALID_OPERATION;
    return err_status;
}

mbed_error_status_t ClawController::retractToolTip() {
    mbed_error_status_t err_status = r_tooltipServo.set_position(m_tooltipRetractedAngle_Degrees) ? MBED_SUCCESS : MBED_ERROR_INVALID_OPERATION;
    return err_status;
}

mbed_error_status_t ClawController::update(){
    if(getGripForce_Newtons() > m_max_force_fsr_newtons){
        switch(getControlMode()) {
            case t_actuatorControlMode::motorPower:
                if(ActuatorController::getMotorPower_Percentage() > 0.0){
                    return setMotorPower_Percentage(0.0);
                }
            case t_actuatorControlMode::velocity:
                if(ActuatorController::getVelocity_DegreesPerSec() > 0.0){
                    return setGapVelocity_CmPerSec(0.0);
                }
                
            //TODO: Determine how to handle angle control mode

            // case t_actuatorControlMode::position:
            //     if(ActuatorController::getAngle_Degrees() > 0.0){
            //         return setGapDistance_Cm(0.0);
            //     }
            default: 
                return MBED_ERROR_INVALID_ARGUMENT;
        }
    }
    return ActuatorController::update();
}

float ClawController::getGapVelocity_CmPerSec() {
    return convertShaftVelocityDegreesToGapVelocityCm(getVelocity_DegreesPerSec());
}
float ClawController::getGapDistance_Cm() {
    return convertShaftPositionDegreesToGapCm(getAngle_Degrees());
}

float ClawController::getGripForce_Newtons() {
    return r_forceSensor.get_force_newtons();
}

mbed_error_status_t ClawController::runPositionCalibration() {
    Timer calibrationTimer;

    ActuatorController::t_actuatorControlMode prevControlMode = getControlMode();
        
    if (m_mutex.trylock_for(1000)) {

        calibrationTimer.start();

        setControlMode(ActuatorController::motorPower);
        setMotorPower_Percentage(0.5);

        while (!isLimSwitchMaxTriggered() && calibrationTimer.read() < m_calibrationTimeout_Seconds)
        {
            update();
            ThisThread::sleep_for(2);
        }

        setMotorPower_Percentage(0.0);

        // Settle
        ThisThread::sleep_for(750);
        resetEncoder();

        setControlMode(prevControlMode);
    }
    else {
        return MBED_ERROR_MUTEX_LOCK_FAILED;
    }

    m_mutex.unlock();

    return MBED_SUCCESS;
}

float ClawController::convertShaftPositionDegreesToGapCm(float shaftPosition_Degrees) {
    return (6.363885761e-7)*(shaftPosition_Degrees*shaftPosition_Degrees) - (8.793434733e-3)*shaftPosition_Degrees + 15.80749897;
}

float ClawController::convertShaftVelocityDegreesToGapVelocityCm(float shaftPosition_DegreesPerSec) {
    return 2*(6.363885761e-7)*shaftPosition_DegreesPerSec - 8.793434733e-3;
}

float ClawController::convertGapCmToShaftPositionDegrees(float gap_cm) {
    return (1.573564198)*(gap_cm*gap_cm)- (158.4968661)*gap_cm + 2119.701587;
    // return (8.282382533e-3)*(gap_cm*gap_cm*gap_cm*gap_cm) - (2.986760459e-1)*(gap_cm*gap_cm*gap_cm) + (5.007842722)*(gap_cm*gap_cm) - (171.560244)*gap_cm + 2127.848743; // High precision 
}

float ClawController::convertGapVelocityCmToShaftVelocityDegrees(float gap_cmPerSec) {
    return 2*(1.573564198)*gap_cmPerSec - 158.4968661;
    // return 4*(8.282382533e-3)*(gap_cmPerSec*gap_cmPerSec*gap_cmPerSec) - 3*(2.986760459e-1)*(gap_cmPerSec*gap_cmPerSec) + 2*(5.007842722)*gap_cmPerSec - 171.560244; // High precision 
}