#pragma once
#include <QString>
#include <QtGlobal>

// One row in the "utilities" table: a room's electricity/water meter
// readings and rates for one calendar month ("yyyy-MM").
// Units: electricity in kWh, water in cubic meters (m3).
struct Utility {
    int id = -1;
    int roomId = -1;
    QString month;

    double elecPrev = 0;
    double elecCurr = 0;
    double elecRate = 0.25;  // $ per kWh

    double waterPrev = 0;
    double waterCurr = 0;
    double waterRate = 1.00; // $ per m3

    double exchangeRate = 4100; // KHR per 1 USD

    double elecUnits() const  { return qMax(0.0, elecCurr - elecPrev); }
    double waterUnits() const { return qMax(0.0, waterCurr - waterPrev); }
    double elecCost() const   { return elecUnits() * elecRate; }
    double waterCost() const  { return waterUnits() * waterRate; }
    double totalCost() const  { return elecCost() + waterCost(); }
};
