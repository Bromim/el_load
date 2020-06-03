#ifndef PID_H_
#define PID_H_
#include <Arduino.h>

#define NORMAL 0
#define REVERSE 1
#define ON_ERROR 0
#define ON_RATE 1

class PID {
	public:
		PID();
		PID(double new_kp, double new_ki, double new_kd, int16_t new_dt = 100);		// kp, ki, kd, dt
		double setpoint = 0;		// заданная величина, которую должен поддерживать регулятор
		double input = 0;			// сигнал с датчика (например температура, которую мы регулируем)
		double output = 0;		// выход с регулятора на управляющее устройство (например величина ШИМ или угол поворота серво)
		double getResult();												// возвращает новое значение при вызове (если используем свой таймер с периодом dt!)	
		double getResult(double new_setpoint, double new_input);		// принимает setpoint и input
		double getResultTimer();											// возвращает новое значение не ранее, чем через dt миллисекунд (встроенный таймер с периодом dt)
		double getResultTimer(double new_setpoint, double new_input);	// тож самое, но принимает setpoint и input

		void setDirection(uint8_t direction);						// направление регулирования: NORMAL (0) или REVERSE (1)
		void setMode(uint8_t mode);									// режим: работа по входной ошибке ON_ERROR (0) или по изменению ON_RATE (1)
		void setLimits(int min_output, int max_output);				// лимит выходной величины (например для ШИМ ставим 0-255)
		void setDt(int16_t new_dt);									// установка времени дискретизации (для getResultTimer)
		void tune(double new_kp, double new_ki, double new_kd);		// перенастройка коэффициентов (П, И, Д)

	private:
		double _kp = 0.0;
		double _ki = 0.0;
		double _kd = 0.0;
		int16_t _dt = 0;
		boolean _mode = 0, _direction = 0;	
		int _minOut = 0, _maxOut = 255;	
		double dt_s = 0.0;
		double kd_dt_s = 0;
		double ki_dt_s = 0;
		double prevInput = 0.0;
		double integral = 0.0;
		uint32_t pidTimer = 0;
};

#endif
