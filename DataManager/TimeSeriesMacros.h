#pragma once
#include "TimeSeriesRegistry.h"
#include <imgui.h>
#include <implot.h>

// Macro for registering a time series
#define REGISTER_TIME_SERIES(id, name, ylabel, getter, color, marker) \
    namespace { \
        struct TimeSeriesRegistrar_##id { \
            TimeSeriesRegistrar_##id() { \
                DataManager::TimeSeriesDefinition def; \
                def.id = #id; \
                def.displayName = name; \
                def.yAxisLabel = ylabel; \
                def.valueGetter = getter; \
                def.color = color; \
                def.marker = marker; \
                def.markerSize = 2.0f; \
                def.enabledByDefault = true; \
                DataManager::TimeSeriesRegistry::GetInstance().RegisterTimeSeries(def); \
            } \
        }; \
        static TimeSeriesRegistrar_##id _ts_reg_##id; \
    }

// Macro for registering a plot window - fixed to handle variable arguments properly
#define REGISTER_PLOT_WINDOW(windowName, plotTitle, ...) \
    namespace { \
        struct PlotWindowRegistrar_##windowName { \
            PlotWindowRegistrar_##windowName() { \
                DataManager::PlotWindowDefinition def; \
                def.windowName = #windowName; \
                def.plotTitle = plotTitle; \
                def.seriesIds = {__VA_ARGS__}; \
                def.legendLocation = ImPlotLocation_NorthWest; \
                def.legendOutside = true; \
                def.ticksEvery = 12; \
                def.showControls = true; \
                DataManager::TimeSeriesRegistry::GetInstance().RegisterPlotWindow(def); \
            } \
        }; \
        static PlotWindowRegistrar_##windowName _pw_reg_##windowName; \
    }