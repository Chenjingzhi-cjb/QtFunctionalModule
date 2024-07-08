#ifndef EIGEN_CALCULATOR_HPP
#define EIGEN_CALCULATOR_HPP

#include <iostream>
#include <vector>
#include <cmath>

#include <Eigen/Dense>
#include <unsupported/Eigen/NonLinearOptimization>
#include <unsupported/Eigen/NumericalDiff>


class EigenCalculator {
public:
    EigenCalculator() = default;

    ~EigenCalculator() = default;

public:
    /**
     * @brief 计算数组的标准差
     *
     * @param x 数组
     * @return 标准差
     */
    double calculateStandardDeviation(const std::vector<double> &x) {
        double mean = std::accumulate(x.begin(), x.end(), 0.0) / x.size();
        double sq_sum = std::inner_product(x.begin(), x.end(), x.begin(), 0.0, std::plus<double>(),
                                           [mean](double xi, double xi_sq) {
                                               return (xi - mean) * (xi - mean);
                                           });

        return std::sqrt(sq_sum / x.size());
    }

    /**
     * @brief 多项式拟合
     *
     * @param x 拟合输入数组 x
     * @param y 拟合输入数组 y
     * @param order 多项式的阶数
     * @return 多项式拟合的结果参数
     */
    Eigen::VectorXd fitPoly(const std::vector<double> &x, const std::vector<double> &y, int order) {
        Eigen::MatrixXd A(x.size(), order + 1);
        Eigen::VectorXd b(x.size());

        for (size_t i = 0; i < x.size(); ++i) {
            b(i) = y[i];
            for (int j = 0; j <= order; ++j) {
                A(i, j) = std::pow(x[i], j);
            }
        }

        Eigen::VectorXd coeff = A.colPivHouseholderQr().solve(b);

        // std::cout << "Fitted coefficients: " << coeff.transpose() << std::endl;

        return coeff;
    }

    /**
     * @brief 多项式预测
     *
     * @param coeff 多项式拟合的结果参数
     * @param x 预测输入数据 x
     * @return 多项式预测输出数据 y
     */
    double predictPoly(const Eigen::VectorXd &coeff, double &x) {
        double y = 0;

        for (int i = 0; i < coeff.size(); ++i) {
            y += coeff[i] * std::pow(x, i);
        }

        return y;
    }

    /**
     * @brief 估算高斯拟合的初始标准差参数
     *
     * @param x 拟合输入数组 x
     * @return 估算得到的用于高斯拟合的初始标准差参数
     */
    double estimateGaussInitialC(const std::vector<double> &x) {
        double min_x = *std::min_element(x.begin(), x.end());
        double max_x = *std::max_element(x.begin(), x.end());
        return (max_x - min_x) / 3.0;  // 取数据范围的三分之一
    }

    /**
     * @brief 高斯拟合
     *
     *     参数: (a - 峰值高度, b - 峰值位置, c - 标准差)
     *     公式: f(x) = a * exp((-1 * (x - b)^2) / (2 * c^2))
     *
     * @param x 拟合输入数组 x
     * @param y 拟合输入数组 y
     * @param initial_guess 拟合输入初始值
     * @return 高斯拟合的结果参数
     */
    Eigen::VectorXd fitGaussian(const std::vector<double> &x,
                                const std::vector<double> &y,
                                const Eigen::VectorXd &initial_guess) {
        GaussFunctor functor(x, y);
        Eigen::NumericalDiff<GaussFunctor> num_diff(functor);
        Eigen::LevenbergMarquardt<Eigen::NumericalDiff<GaussFunctor>> lm(num_diff);

        Eigen::VectorXd params = initial_guess;
        lm.minimize(params);

        // std::cout << "Fitted parameters: " << params.transpose() << std::endl;

        return params;
    }

    /**
     * @brief 高斯预测
     *
     * @param params 高斯拟合的结果参数
     * @param x 预测输入数据 x
     * @return 高斯预测输出数据 y
     */
    double predictGaussian(Eigen::VectorXd params, double x) {
        return params[0] * std::exp(-1 * std::pow(x - params[1], 2) / (2 * std::pow(params[2], 2)));
    }

private:
    struct GaussFunctor {
        const std::vector<double> &m_x;
        const std::vector<double> &m_y;

        GaussFunctor(const std::vector<double> &x, const std::vector<double> &y)
            : m_x(x), m_y(y) {}

        using Scalar = double;
        using InputType = Eigen::VectorXd;
        using ValueType = Eigen::VectorXd;
        using JacobianType = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>;

        enum {
            InputsAtCompileTime = Eigen::Dynamic,
            ValuesAtCompileTime = Eigen::Dynamic
        };

        int operator()(const Eigen::VectorXd &b, Eigen::VectorXd &fvec) const {
            for (int i = 0; i < m_x.size(); ++i) {
                fvec[i] = m_y[i] - b[0] * std::exp(-std::pow(m_x[i] - b[1], 2) / (2 * std::pow(b[2], 2)));
            }
            return 0;
        }

        int inputs() const { return 3; }
        int values() const { return (int) m_x.size(); }
    };
};


#endif // EIGEN_CALCULATOR_HPP
