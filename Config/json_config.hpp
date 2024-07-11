#ifndef JSON_CONFIG_HPP
#define JSON_CONFIG_HPP

#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QString>

#include <iostream>
#include <string>
#include <vector>

#include "json.hpp"


using json_t = nlohmann::json;


class JsonConfigParam {
public:
    std::string exec_path;

    int aa;
    std::string bb;
    std::vector<int> cc;
    std::vector<std::string> dd;
    std::vector<json_t> ee;
};


class JsonConfigReader : public JsonConfigParam {
private:
    JsonConfigReader() {
        initConfig();
    }

    ~JsonConfigReader() = default;

public:
    static JsonConfigReader &get_instance() {
        static JsonConfigReader instance;
        return instance;
    }

    void loadConfig(const QString &config_path) {
        QFile config_file(config_path);
        if (config_file.open(QIODevice::ReadOnly)) {
            QTextStream in(&config_file);
            std::string config_info = in.readAll().toStdString();
            config_file.close();

            try {
                json_t config_json_obj = json_t::parse(config_info);
                aa = config_json_obj.at("aa").get<int>();
                bb = config_json_obj.at("bb").get<std::string>();
                bb = config_json_obj.at("cc").get<std::vector<int>>();
                bb = config_json_obj.at("dd").get<std::vector<std::string>>();
                cc = config_json_obj.at("ee").get<std::vector<json_t>>();
            } catch (json_t::exception &e) {
                std::cout << "Failed to parse the config file: " << e.what() << std::endl;
            }
        } else {
            std::cout << "Failed to open the config file." << std::endl;
        }
    }

private:
    void initConfig() {
        exec_path = QCoreApplication::applicationDirPath().toStdString();
    }
};


class JsonConfigWriter : public JsonConfigParam {
public:
    JsonConfigWriter() = default;

    ~JsonConfigWriter() = default;

public:
    void saveConfig(const QString &config_path) {
        json_t config_json_obj;
        config_json_obj["aa"] = aa;
        config_json_obj["bb"] = bb;
        config_json_obj["cc"] = cc;
        config_json_obj["dd"] = dd;
        config_json_obj["ee"] = ee;

        QFile config_file(config_path);
        if (config_file.open(QIODevice::WriteOnly)) {
            QTextStream out(&config_file);

            try {
                out << QString::fromStdString(config_json_obj.dump(4));
                config_file.close();
            } catch (json_t::exception &e) {
                std::cout << "Failed to serialize the config data: " << e.what() << std::endl;
            }
        } else {
            std::cout << "Failed to open the config file for writing." << std::endl;
        }
    }
};


#endif // JSON_CONFIG_HPP
