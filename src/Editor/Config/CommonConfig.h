#pragma once

//-- includes -----
#include "CommonConfigFwd.h"
#include "MulticastDelegate.h"

#include <memory>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "glm/ext/quaternion_double.hpp"
#include "glm/ext/vector_double3.hpp"

// Forward declarations
class EventBus;
struct PropertyChangedEvent;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996) // This function or variable may be unsafe
#pragma warning(disable : 4244) // 'return': conversion from 'const int64_t' to 'float', possible loss of data
#pragma warning(disable : 4715) // configuru::Config::operator[]': not all control paths return a value
#endif
#include <configuru.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

//-- definitions -----
/*
	Note that CommonConfig is an abstract class because it has 2 pure virtual functions.
	Child classes must add public member variables that store the config data,
	as well as implement writeToJSON and readFromJSON that use pt[key]= value and
	pt.get_or<type>(), respectively, to convert between member variables and the
	property tree.
*/
class ConfigPropertyChangeSet
{
public:
	ConfigPropertyChangeSet& addPropertyName(const std::string& propertyName);
	bool hasPropertyName(const std::string& propertyName) const;
	const std::set<std::string>& getSet() const { return m_changedProperties; }

private:
	std::set<std::string> m_changedProperties;
};

class CommonConfig : public std::enable_shared_from_this<CommonConfig>
{
public:
	CommonConfig(const std::string& configName= std::string("CommonConfig"));

	void addChildConfig(std::shared_ptr<CommonConfig> childConfig);
	std::shared_ptr<CommonConfig> getChildConfig(const std::string& configName) const;
	void removeChildConfig(std::shared_ptr<CommonConfig> childConfig);
	virtual void notifyPropertyChanged(const ConfigPropertyChangeSet& changedPropertySet);
	virtual bool wantsConfigSerialization() const { return true; }
	virtual bool wantsSaveForPropertyChange(const ConfigPropertyChangeSet& changedPropertySet) const { return true; }

	// Legacy delegate-based property change notification
	MulticastDelegate<void(CommonConfigPtr configPtr, const ConfigPropertyChangeSet& changedPropertySet)>
		OnPropertyChanged;

	void setAutoSaveCooldownDuration(float cooldownDuration);
	float getAutoSaveCooldownDuration() const { return m_autoSaveCooldownDuration; }
	void updateAutoSave(float deltaSeconds);
	// Save now if a change is waiting out the cooldown, so unloading the config
	// does not drop it
	void flushPendingAutoSave();

	const std::string& getConfigName() const { return m_configName; }
	const std::filesystem::path getDefaultConfigPath() const;
	const std::filesystem::path& getLoadedConfigPath() const { return m_configFullFilePath; }
	void save();
	void save(const std::filesystem::path& path);
	bool load();
	bool load(const std::filesystem::path& path);

	virtual configuru::Config writeToJSON();                // Implement by each device class' own Config
	virtual void readFromJSON(const configuru::Config& pt); // Implement by each device class' own Config

	template <typename t_value_type>
	static void writeStdValueVector(configuru::Config& pt, const std::string& arrayName,
									const std::vector<t_value_type>& vector)
	{
		auto configArray= configuru::Config::array();

		for (auto it= vector.begin(); it != vector.end(); it++)
		{
			configArray.push_back(*it);
		}

		pt[arrayName]= configArray;
	}
	template <typename t_value_type>
	static void readStdValueVector(const configuru::Config& pt, const std::string& arrayName,
								   std::vector<t_value_type>& vector)
	{
		const auto& configArray= pt[arrayName].as_array();

		vector.clear();
		for (auto it= configArray.begin(); it != configArray.end(); it++)
		{
			vector.push_back(it->get<t_value_type>());
		}
	}

	template <class t_object_type>
	static void writeStdConfigVector(configuru::Config& pt, const std::string& arrayName,
									 const std::vector<std::shared_ptr<t_object_type>>& vector)
	{
		auto configArray= configuru::Config::array();

		for (auto it= vector.begin(); it != vector.end(); it++)
		{
			std::shared_ptr<t_object_type> childConfig= *it;
			configuru::Config pt= childConfig->writeToJSON();

			configArray.push_back(pt);
		}

		pt[arrayName]= configArray;
	}
	template <class t_object_type>
	static void readStdConfigVector(const configuru::Config& pt, const std::string& arrayName,
									std::vector<std::shared_ptr<t_object_type>>& vector)
	{
		const auto& configArray= pt[arrayName].as_array();

		vector.clear();
		for (auto it= configArray.begin(); it != configArray.end(); it++)
		{
			std::shared_ptr<t_object_type> childConfig= std::make_shared<t_object_type>();
			childConfig->readFromJSON(*it);

			vector.push_back(childConfig);
		}
	}

	template <typename t_value_type, size_t length>
	static void writeStdArray(configuru::Config& pt, const std::string& arrayName,
							  const std::array<t_value_type, length>& array)
	{
		auto configArray= configuru::Config::array();

		for (size_t i= 0; i < length; i++)
		{
			configArray.push_back(array[i]);
		}

		pt[arrayName]= configArray;
	}
	template <typename t_value_type, size_t length>
	static void readStdArray(const configuru::Config& pt, const std::string& arrayName,
							 std::array<t_value_type, length>& array)
	{
		const auto& configArray= pt[arrayName].as_array();

		size_t index= 0;
		for (auto it= configArray.begin(); it != configArray.end(); it++)
		{
			if (index < length)
			{
				array[index]= it->get<t_value_type>();
				index++;
			}
			else
			{
				break;
			}
		}
	}

	template <typename t_value_type>
	static void writeStdMap(configuru::Config& pt, const std::string& mapName,
							const std::map<std::string, t_value_type>& nameValueMap)
	{
		pt[mapName]= configuru::Config::object();

		for (auto it= nameValueMap.begin(); it != nameValueMap.end(); ++it)
		{
			const std::string& name= it->first;
			const t_value_type& value= it->second;

			pt[mapName][name]= value;
		}
	}
	template <typename t_value_type>
	static void readStdMap(const configuru::Config& pt, const std::string& mapName,
						   std::map<std::string, t_value_type>& nameValueMap)
	{
		if (pt.has_key(mapName))
		{
			const configuru::Config::ConfigObject& configObject= pt[mapName].as_object();

			nameValueMap.clear();
			for (configuru::Config::ConfigObject::const_iterator it= configObject.begin(); it != configObject.end();
				 ++it)
			{
				const std::string& name= it.key();
				const configuru::Config& config= it.value();
				const t_value_type& value= config.get<t_value_type>();

				nameValueMap.insert({name, value});
			}
		}
	}

	template <typename t_value_type, size_t N>
	static void writeStdArrayMap(configuru::Config& pt, const std::string& mapName,
								 const std::map<std::string, std::array<t_value_type, N>>& nameValueMap)
	{
		pt[mapName]= configuru::Config::object();

		for (auto it= nameValueMap.begin(); it != nameValueMap.end(); ++it)
		{
			const std::string& name= it->first;
			const std::array<t_value_type, N>& valueArray= it->second;

			pt[mapName][name]= configuru::Config::array(valueArray);
		}
	}

	template <typename t_value_type, size_t N>
	static void readStdArrayMap(const configuru::Config& pt, const std::string& mapName,
								std::map<std::string, std::array<t_value_type, N>>& nameValueMap)
	{
		if (pt.has_key(mapName))
		{
			const configuru::Config::ConfigObject& configObject= pt[mapName].as_object();

			nameValueMap.clear();
			for (configuru::Config::ConfigObject::const_iterator it= configObject.begin(); it != configObject.end();
				 ++it)
			{
				const std::string& name= it.key();
				const configuru::Config& configValue= it.value();
				if (configValue.is_array() && configValue.array_size() == N)
				{
					std::array<t_value_type, N> valueArray;

					for (int i= 0; i < N; i++)
					{
						valueArray[i]= configValue[i].get<t_value_type>();
					}

					nameValueMap.insert({name, valueArray});
				}
			}
		}
	}

	static void writeMonoTrackerIntrinsics(configuru::Config& pt, const struct MikanMonoIntrinsics& tracker_intrinsics);
	static void readMonoTrackerIntrinsics(const configuru::Config& pt, struct MikanMonoIntrinsics& tracker_intrinsics);

	static void writeStereoTrackerIntrinsics(configuru::Config& pt,
											 const struct MikanStereoIntrinsics& tracker_intrinsics);
	static void readStereoTrackerIntrinsics(const configuru::Config& pt,
											struct MikanStereoIntrinsics& tracker_intrinsics);

	static void writeDistortionCoefficients(configuru::Config& pt, const char* coefficients_name,
											const struct MikanDistortionCoefficients* coefficients);
	static void readDistortionCoefficients(const configuru::Config& pt, const char* coefficients_name,
										   struct MikanDistortionCoefficients* outCoefficients,
										   const struct MikanDistortionCoefficients* defaultCoefficients);

	static void writeMatrix3d(configuru::Config& pt, const char* matrix_name, const struct MikanMatrix3d& matrix);
	static void readMatrix3d(const configuru::Config& pt, const char* matrix_name, struct MikanMatrix3d& outMatrix);

	static void writeMatrix43d(configuru::Config& pt, const char* matrix_name, const struct MikanMatrix4x3d& matrix);
	static void readMatrix43d(const configuru::Config& pt, const char* matrix_name, struct MikanMatrix4x3d& outMatrix);

	static void writeMatrix4d(configuru::Config& pt, const char* matrix_name, const struct MikanMatrix4d& matrix);
	static void readMatrix4d(const configuru::Config& pt, const char* matrix_name, struct MikanMatrix4d& outMatrix);

	static void writeMatrix4f(configuru::Config& pt, const char* matrix_name, const struct MikanMatrix4f& matrix);
	static void readMatrix4f(const configuru::Config& pt, const char* matrix_name, struct MikanMatrix4f& outMatrix);

	static void writeQuaderntiond(configuru::Config& pt, const char* quat_name, const struct MikanQuatd& quat);
	static void readQuaterniond(const configuru::Config& pt, const char* quat_name, struct MikanQuatd& outQuat);

	static void writeVector3d(configuru::Config& pt, const char* vector_name, const struct MikanVector3d& vector);
	static void readVector3d(const configuru::Config& pt, const char* vector_name, struct MikanVector3d& outVector);

	static void writeVector3f(configuru::Config& pt, const char* vector_name, const struct MikanVector3f& vector);
	static void readVector3f(const configuru::Config& pt, const char* vector_name, struct MikanVector3f& outVector);

	static void writeVector2f(configuru::Config& pt, const char* vector_name, const struct MikanVector2f& vector);
	static void readVector2f(const configuru::Config& pt, const char* vector_name, struct MikanVector2f& outVector);

	static void writeVector2i(configuru::Config& pt, const char* vector_name, const struct MikanVector2i& vector);
	static void readVector2i(const configuru::Config& pt, const char* vector_name, struct MikanVector2i& outVector);

	static void writeRotator3f(configuru::Config& pt, const char* vector_name, const struct MikanRotator3f& vector);
	static void readRotator3f(const configuru::Config& pt, const char* vector_name, struct MikanRotator3f& outVector);

	static void writeQuatf(configuru::Config& pt, const char* quat_name, const struct MikanQuatf& quat);
	static void readQuatf(const configuru::Config& pt, const char* quat_name, struct MikanQuatf& outQuat);

protected:
	std::vector<CommonConfigPtr> m_childConfigs;
	void onChildConfigPropertyChanged(CommonConfigPtr configPtr, const ConfigPropertyChangeSet& changedPropertySet);

	std::string m_configName;
	std::filesystem::path m_configFullFilePath;

	float m_autoSaveCooldownDuration= -1.f;
	float m_autoSaveCooldownTimer= -1.f;
};