#ifndef JSON_CONFIG_HPP
#define JSON_CONFIG_HPP

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QString>

#include <iostream>
#include <string>
#include <vector>

#include "json.hpp"
#include "logger.hpp"


using json_t = nlohmann::ordered_json;


class JsonConfig {
protected:
    JsonConfig()
        : exec_path(QCoreApplication::applicationDirPath()),
          m_config_valid(false) {}

    ~JsonConfig() = default;

public:
    void loadConfig(const QString &config_path) {
        std::string config_filename = QFileInfo(config_path).fileName().toStdString();

        QFile config_file(config_path);
        if (!config_file.exists()) {
            m_config_valid = false;
            Log_ERROR_M("Config", "Config file \"{}\" does not exist.", config_filename);
            return;
        }

        if (!config_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            m_config_valid = false;
            Log_ERROR_M("Config", "Failed to open the config file \"{}\" for reading.", config_filename);
            return;
        }

        try {
            QByteArray bytes = config_file.readAll();
            m_json_data = json_t::parse(bytes.begin(), bytes.end());

            m_config_path = config_path;
            m_config_valid = true;
            Log_INFO_M("Config", "Config \"{}\" loading complete!", config_filename);
        } catch (json_t::exception &e) {
            m_config_valid = false;
            Log_ERROR_M("Config", "Failed to parse the config file \"{}\": {}", config_filename, e.what());
        }

        config_file.close();
    }

    void loadConfigExecPath(const QString &config_path) {
        QString path = exec_path + QDir::separator() + config_path;
        loadConfig(QDir::cleanPath(path));
    }

    void saveConfig(const QString &config_path) {
        std::string config_filename = QFileInfo(config_path).fileName().toStdString();

        std::string dump_string;
        try {
            dump_string = m_json_data.dump(4);
        } catch (const json_t::exception &e) {
            Log_ERROR_M("Config", "Serialization failed: {}", e.what());
            return;
        }

        QFile config_file(config_path);
        if (!config_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            Log_ERROR_M("Config", "Failed to open the config file \"{}\" for writing.", config_filename);
            return;
        }

        config_file.write(dump_string.c_str(), dump_string.length());
        config_file.close();

        Log_INFO_M("Config", "Config \"{}\" saving complete!", config_filename);
    }

    void saveConfigExecPath(const QString &config_path) {       
        QString path = exec_path + QDir::separator() + config_path;
        saveConfig(QDir::cleanPath(path));
    }

    void updateConfig() {
        if (m_config_valid) {
            saveConfig(m_config_path);
        } else {
            Log_ERROR_M("Config", "Config not loaded or original config file invalid!");
        }
    }

    QString getExecPath() const {
        return exec_path;
    }

    json_t &getJsonData() {
        return m_json_data;
    }

    bool isLoadConfigError() const {
        return (!m_config_valid);
    }

protected:
    QString exec_path;

    json_t m_json_data;
    QString m_config_path;
    bool m_config_valid;
};


class JsonConfigExample : public JsonConfig {
private:
    JsonConfigExample()
        : JsonConfig() {}

    ~JsonConfigExample() = default;

public:
    JsonConfigExample(const JsonConfigExample &) = delete;
    JsonConfigExample &operator=(const JsonConfigExample &) = delete;

    static JsonConfigExample &getInstance() {
        static JsonConfigExample instance;
        return instance;
    }

public:
    // key-value(int)
    void setKeyValueInt(int value) {
        m_json_data["KeyValueInt"] = value;
    }

    int getKeyValueInt() {
        return m_json_data.at("KeyValueInt").get<int>();
    }

    // key-value(string)
    void setKeyValueString(const std::string &value) {
        m_json_data["KeyValueString"] = value;
    }

    std::string getKeyValueString() {
        return m_json_data.at("KeyValueString").get<std::string>();
    }

    // key-vector(int)
    void setKeyVectorInt(const std::vector<int> &value) {
        m_json_data["KeyVectorInt"] = value;
    }

    std::vector<int> getKeyVectorInt() {
        return m_json_data.at("KeyVectorInt").get<std::vector<int>>();
    }

    // key-json
    void setKeyJson(const json_t &value) {
        m_json_data["KeyJson"] = value;
    }

    json_t getKeyJson() {
        return m_json_data.at("KeyJson").get<json_t>();
    }
};


#endif // JSON_CONFIG_HPP
