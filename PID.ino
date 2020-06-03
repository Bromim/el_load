#include "Arduino.h"
#include "PID.h"

PID::PID() {}

PID::PID(double new_kp, double new_ki, double new_kd, int16_t new_dt) {
	_dt = new_dt;
	pidTimer = (long)millis() + _dt;
	tune(new_kp, new_ki, new_kd);
}

void PID::setDirection(uint8_t direction) {
	_direction = direction;
	tune(_kp, _ki, _kd);
}

void PID::setMode(uint8_t mode) {
	_mode = mode;
}

void PID::setLimits(int min_output, int max_output) {	
	_minOut = min_output;
	_maxOut = max_output;
}

void PID::setDt(int16_t new_dt) {	
	_dt = new_dt;
	dt_s = _dt / 1000.0;
	kd_dt_s = _kd / dt_s;
	ki_dt_s = _ki * dt_s;
}

void PID::tune(double new_kp, double new_ki, double new_kd) {
	int8_t sign = _direction ? -1 : 1;
	_kp = new_kp * sign;
	_ki = new_ki * sign;
	_kd = new_kd * sign;	
	setDt(_dt);
}



double PID::getResult() {
	double error = setpoint - input;				// ошибка регулирования
	double delta_input = input - prevInput;		// изменение входного сигнала
	prevInput = input;

	if (!_mode) output += (double)error * _kp;	// пропорционально ошибке регулирования
	else output -= (double)delta_input * _kp;	// пропорционально изменению входного сигнала
 
	output -= (double)delta_input * kd_dt_s;		// дифференциальная составляющая	
	integral += (double)error * ki_dt_s;			// расчёт интегральной составляющей
	output += integral;							// прибавляем

	output = constrain(output, _minOut, _maxOut);    // ограничиваем
	return output;
}

double PID::getResult(double new_setpoint, double new_input) {
	setpoint = new_setpoint;
	input = new_input;
	return PID::getResult();
}

double PID::getResultTimer() {
	if ((long)millis() - pidTimer >= _dt) {
		pidTimer = millis();
		return PID::getResult();
	} else {
		return output;
	}
}

double PID::getResultTimer(double new_setpoint, double new_input) {
	setpoint = new_setpoint;
	input = new_input;
	return PID::getResultTimer();
}
