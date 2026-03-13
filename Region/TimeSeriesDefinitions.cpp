// TimeSeriesDefinitions.cpp - Working version without broken macros
#include <imgui.h>
#include <implot.h>
#include "../Simulator/Simulator.h"
#include "../DataManager/DataManager.h"
#include "../Logger/Logger.h"
#include "Region.h"
// Remove the TimeSeriesMacros.h include since it's not working

// Helper function to safely get region and cast it
static Region::Region* GetRegionSafe() {
    void* regionPtr = Simulator::GetRegion();
    return regionPtr ? static_cast<Region::Region*>(regionPtr) : nullptr;
}

// Helper function to reduce boilerplate
static void RegisterTS(
    const std::string& id,
    const std::string& displayName,
    const std::string& yAxisLabel,
    std::function<float()> valueGetter,
    ImVec4 color,
    ImPlotMarker marker)
{
    DataManager::TimeSeriesDefinition def;
    def.id = id;
    def.displayName = displayName;
    def.yAxisLabel = yAxisLabel;
    def.valueGetter = valueGetter;
    def.color = color;
    def.marker = marker;
    def.markerSize = 2.0f;
    def.enabledByDefault = true;
    DataManager::TimeSeriesRegistry::GetInstance().RegisterTimeSeries(def);
}

// Static registration objects for time series
namespace {
    struct RegisterAllTimeSeries {
        RegisterAllTimeSeries() {
            // ===== ECONOMIC INDICATORS =====
            RegisterTS("gdp", "GDP", "Value",
                []() {
                    auto region = GetRegionSafe();
                    return region ? static_cast<float>(region->getGDP()) : 0.0f;
                },
                ImVec4(0.2f, 0.7f, 0.2f, 1.0f),
                ImPlotMarker_Circle);

            RegisterTS("household_expenditure", "House Expend", "Value",
                []() {
                    auto region = GetRegionSafe();
                    return region ? static_cast<float>(region->getTotalHouseholdExpenditure()) : 0.0f;
                },
                ImVec4(0.8f, 0.4f, 0.2f, 1.0f),
                ImPlotMarker_Square);

            // ===== COMMERCIAL BANK METRICS =====
            RegisterTS("commercial_bank_deposits", "CB Deposits", "Value",
                []() {
                    auto region = GetRegionSafe();
                    return region ? static_cast<float>(region->getTotalCommercialBankDeposits()) : 0.0f;
                },
                ImVec4(0.3f, 0.7f, 0.9f, 1.0f),
                ImPlotMarker_Circle);

            RegisterTS("commercial_bank_loans", "CB Loans", "Value",
                []() {
                    auto region = GetRegionSafe();
                    return region ? static_cast<float>(region->getTotalCommercialBankLoans()) : 0.0f;
                },
                ImVec4(0.9f, 0.3f, 0.3f, 1.0f),
                ImPlotMarker_Square);

            RegisterTS("commercial_bank_cash", "CB Cash", "Value",
                []() {
                    auto region = GetRegionSafe();
                    return region ? static_cast<float>(region->getTotalCommercialBankCash()) : 0.0f;
                },
                ImVec4(0.3f, 0.9f, 0.3f, 1.0f),
                ImPlotMarker_Diamond);

            RegisterTS("commercial_bank_reserves", "CB Reserves", "Value",
                []() {
                    auto region = GetRegionSafe();
                    return region ? static_cast<float>(region->getTotalCommercialBankReserves()) : 0.0f;
                },
                ImVec4(0.7f, 0.5f, 0.9f, 1.0f),
                ImPlotMarker_Up);

            // ===== CENTRAL BANK METRICS =====
            RegisterTS("total_reserves", "Total Reserves", "Value",
                []() {
                    auto region = GetRegionSafe();
                    return region ? static_cast<float>(region->getCentralBankTotalReserves()) : 0.0f;
                },
                ImVec4(0.1f, 0.5f, 0.9f, 1.0f),
                ImPlotMarker_Circle);

            RegisterTS("total_money", "Total Money", "Value",
                []() {
                    auto region = GetRegionSafe();
                    return region ? static_cast<float>(region->getCentralBankTotalMoney()) : 0.0f;
                },
                ImVec4(0.9f, 0.5f, 0.1f, 1.0f),
                ImPlotMarker_Square);

            // ===== PRODUCTION METRICS =====
            RegisterTS("total_production", "Total Production", "Value",
                []() {
                    auto region = GetRegionSafe();
                    return region ? static_cast<float>(region->getTotalProduction()) : 0.0f;
                },
                ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
                ImPlotMarker_Circle);

            RegisterTS("assisted_production", "Assisted Production", "Value",
                []() {
                    auto region = GetRegionSafe();
                    return region ? static_cast<float>(region->getTotalAssistedProduction()) : 0.0f;
                },
                ImVec4(0.8f, 0.8f, 0.2f, 1.0f),
                ImPlotMarker_Square);

            RegisterTS("actual_production", "Actual Production", "Value",
                []() {
                    auto region = GetRegionSafe();
                    return region ? static_cast<float>(region->getTotalActualProduction()) : 0.0f;
                },
                ImVec4(0.2f, 0.2f, 0.8f, 1.0f),
                ImPlotMarker_Diamond);

            // ===== LABOR MARKET METRICS =====
            RegisterTS("unemployment_rate", "Unemployment %", "Percentage",
                []() {
                    auto region = GetRegionSafe();
                    return region ? static_cast<float>(region->GetUnemploymentRate() * 100.0) : 0.0f;
                },
                ImVec4(0.8f, 0.2f, 0.2f, 1.0f),
                ImPlotMarker_Up);
        }
    } registerAllTimeSeries;

    // ========== REGISTER PLOT WINDOWS ==========
    struct RegisterPlotWindows {
        RegisterPlotWindows() {
            auto& registry = DataManager::TimeSeriesRegistry::GetInstance();

            // Economic overview
            {
                DataManager::PlotWindowDefinition def;
                def.windowName = "Economic Time Series";
                def.plotTitle = "##EconomicTimeSeries";
                def.seriesIds = { "gdp", "household_expenditure" };
                def.legendLocation = ImPlotLocation_NorthWest;
                def.legendOutside = true;
                def.ticksEvery = 12;
                def.showControls = true;
                registry.RegisterPlotWindow(def);
            }

            // Commercial Bank Metrics
            {
                DataManager::PlotWindowDefinition def;
                def.windowName = "Commercial Bank Metrics";
                def.plotTitle = "##CommercialBankPlot";
                def.seriesIds = { "commercial_bank_deposits", "commercial_bank_loans",
                               "commercial_bank_cash", "commercial_bank_reserves" };
                def.legendLocation = ImPlotLocation_NorthWest;
                def.legendOutside = true;
                def.ticksEvery = 12;
                def.showControls = true;
                registry.RegisterPlotWindow(def);
            }

            // Central Bank Metrics
            {
                DataManager::PlotWindowDefinition def;
                def.windowName = "Central Bank Metrics";
                def.plotTitle = "##CentralBankPlot";
                def.seriesIds = { "total_reserves", "total_money" };
                def.legendLocation = ImPlotLocation_NorthWest;
                def.legendOutside = true;
                def.ticksEvery = 12;
                def.showControls = true;
                registry.RegisterPlotWindow(def);
            }

            // Production Metrics
            {
                DataManager::PlotWindowDefinition def;
                def.windowName = "Production Metrics";
                def.plotTitle = "##ProductionPlot";
                def.seriesIds = { "total_production", "assisted_production", "actual_production" };
                def.legendLocation = ImPlotLocation_NorthWest;
                def.legendOutside = true;
                def.ticksEvery = 12;
                def.showControls = true;
                registry.RegisterPlotWindow(def);
            }

            // Labor Market
            {
                DataManager::PlotWindowDefinition def;
                def.windowName = "Labor Market";
                def.plotTitle = "##LaborMarket";
                def.seriesIds = { "unemployment_rate" };
                def.legendLocation = ImPlotLocation_NorthWest;
                def.legendOutside = true;
                def.ticksEvery = 12;
                def.showControls = true;
                registry.RegisterPlotWindow(def);
            }
        }
    } registerPlotWindows;

} // anonymous namespace

// Force linking and initialization
namespace Region {
    void EnsureTimeSeriesRegistration() {
        static bool initialized = false;
        if (!initialized) {
            initialized = true;
            auto& registry = DataManager::TimeSeriesRegistry::GetInstance();
            auto plots = registry.GetRegisteredPlotWindows();
            Logger::Info("TimeSeriesDefinitions: Forced registration, found " +
                std::to_string(plots.size()) + " plot windows");

            // List them for debugging
            for (const auto& plot : plots) {
                Logger::Debug("  - Registered plot: " + plot.windowName);
            }
        }
    }
}