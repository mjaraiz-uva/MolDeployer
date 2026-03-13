#include "TimeSeriesRegistry.h"
#include "../Logger/Logger.h"
#include "DataManager.h"
#include <algorithm>

namespace DataManager {

    TimeSeriesRegistry& TimeSeriesRegistry::GetInstance() {
        static TimeSeriesRegistry instance;
        return instance;
    }

    void TimeSeriesRegistry::RegisterTimeSeries(const TimeSeriesDefinition& def) {
        if (m_series.find(def.id) != m_series.end()) {
            Logger::Warning("TimeSeriesRegistry: Overwriting existing series: " + def.id);
        }
        m_series[def.id] = def;

        // Initialize data vector with current max months
        int maxMonths = GetMaxSteps();
        m_series[def.id].data.assign(maxMonths, 0.0f);

        Logger::Debug("TimeSeriesRegistry: Registered series: " + def.id);
    }

    void TimeSeriesRegistry::RegisterPlotWindow(const PlotWindowDefinition& def) {
        m_plotWindows.push_back(def);
        Logger::Info("TimeSeriesRegistry: Registered plot window: " + def.windowName +
            " (Total registered: " + std::to_string(m_plotWindows.size()) + ")");
    }

    void TimeSeriesRegistry::UpdateAllSeries(int currentMonth) {
        for (auto& [id, series] : m_series) {
            if (series.valueGetter && currentMonth >= 0 &&
                currentMonth < static_cast<int>(series.data.size())) {
                try {
                    float value = series.valueGetter();
                    series.data[currentMonth] = value;
                }
                catch (const std::exception& e) {
                    Logger::Error("TimeSeriesRegistry: Error updating series " + id +
                        ": " + e.what());
                }
            }
        }

        // Signal new data available
        SetNewDataAvailable(true);
    }

    void TimeSeriesRegistry::ClearAllSeries() {
        int maxMonths = GetMaxSteps();
        for (auto& [id, series] : m_series) {
            series.data.assign(maxMonths, 0.0f);
        }
    }

    void TimeSeriesRegistry::InitializeAllSeries(int maxMonths) {
        for (auto& [id, series] : m_series) {
            series.data.assign(maxMonths, 0.0f);
        }
    }

    const std::vector<float>* TimeSeriesRegistry::GetSeriesData(const std::string& id) const {
        auto it = m_series.find(id);
        if (it != m_series.end()) {
            return &it->second.data;
        }
        return nullptr;
    }

    TimeSeriesDefinition* TimeSeriesRegistry::GetSeriesDefinition(const std::string& id) {
        auto it = m_series.find(id);
        if (it != m_series.end()) {
            return &it->second;
        }
        return nullptr;
    }

    std::vector<std::string> TimeSeriesRegistry::GetAvailableSeriesIds() const {
        std::vector<std::string> ids;
        for (const auto& [id, _] : m_series) {
            ids.push_back(id);
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    std::vector<PlotWindowDefinition> TimeSeriesRegistry::GetRegisteredPlotWindows() const {
        std::vector<PlotWindowDefinition> allWindows = m_plotWindows;
        allWindows.insert(allWindows.end(), m_dynamicPlotWindows.begin(),
            m_dynamicPlotWindows.end());
        return allWindows;
    }

    void TimeSeriesRegistry::AddDynamicPlotWindow(const PlotWindowDefinition& def) {
        m_dynamicPlotWindows.push_back(def);
    }

    void TimeSeriesRegistry::RemoveDynamicPlotWindow(const std::string& windowName) {
        m_dynamicPlotWindows.erase(
            std::remove_if(m_dynamicPlotWindows.begin(), m_dynamicPlotWindows.end(),
                [&windowName](const PlotWindowDefinition& def) {
                    return def.windowName == windowName;
                }),
            m_dynamicPlotWindows.end()
        );
    }

} // namespace DataManager