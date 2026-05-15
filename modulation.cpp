#include <iostream>
#include <vector>
#include <random>
#include <cmath>
#include <complex>
#include <map>
#include <algorithm>
#include <fstream>
#include <iomanip>

class QAMModulator {
public:
    enum Type { QPSK, QAM16, QAM64 };
    
    QAMModulator(Type type) : type(type) {
        generateConstellation();
    }
    
    std::vector<std::complex<double>> modulate(const std::vector<int>& bits) {
        int bitsPerSym = getBitsPerSymbol();
        if (bits.size() % bitsPerSym != 0) {
            throw std::runtime_error("Количество битов должно быть кратно количеству битов на символ");
        }
        
        std::vector<std::complex<double>> symbols;
        for (size_t i = 0; i < bits.size(); i += bitsPerSym) {
            int index = 0;
            for (int j = 0; j < bitsPerSym; ++j) {
                index = (index << 1) | bits[i + j];
            }
            symbols.push_back(constellation[index]);
        }
        return symbols;
    }
    
    const std::map<int, std::complex<double>>& getConstellation() const {
        return constellation;
    }
    
    int getBitsPerSymbol() const {
        switch(type) {
            case QPSK: return 2;
            case QAM16: return 4;
            case QAM64: return 6;
            default: return 0;
        }
    }
    
private:
    Type type;
    std::map<int, std::complex<double>> constellation;
    
    void generateConstellation() {
        constellation.clear();
        switch(type) {
            case QPSK: {
                double E = 1.0 / sqrt(2);
                constellation[0] = std::complex<double>(-E, -E);
                constellation[1] = std::complex<double>(-E,  E);
                constellation[2] = std::complex<double>( E, -E);
                constellation[3] = std::complex<double>( E,  E);
                break;
            }
            case QAM16: {
                std::vector<double> levels = {-3, -1, 1, 3};
                double norm = sqrt(10.0);
                int idx = 0;
                for (double y : levels) {
                    for (double x : levels) {
                        constellation[idx++] = std::complex<double>(x, y) / norm;
                    }
                }
                break;
            }
            case QAM64: {
                std::vector<double> levels = {-7, -5, -3, -1, 1, 3, 5, 7};
                double norm = sqrt(42.0);
                int idx = 0;
                for (double y : levels) {
                    for (double x : levels) {
                        constellation[idx++] = std::complex<double>(x, y) / norm;
                    }
                }
                break;
            }
            
        }
    }
};

class GAUSSChannel {
public:
    GAUSSChannel(double variance = 0.1) : variance(variance), gen(rd()), dist(0.0, sqrt(variance)) {

    }
    
    void setVariance(double var) {
        variance = var;
        dist = std::normal_distribution<double>(0.0, sqrt(variance));
    }
    
    double getVariance() const { return variance; }
    
    std::vector<std::complex<double>> addNoise(const std::vector<std::complex<double>>& symbols) {
        std::vector<std::complex<double>> noisy(symbols.size());
        for (size_t i = 0; i < symbols.size(); ++i) {
            double noiseI = dist(gen);
            double noiseQ = dist(gen);
            noisy[i] = symbols[i] + std::complex<double>(noiseI, noiseQ);
        }
        return noisy;
    }
    
private:
    double variance;
    std::random_device rd;
    std::mt19937 gen{rd()};
    std::normal_distribution<double> dist;
};

class QAMDemodulator {
public:
    QAMDemodulator(const QAMModulator& modulator) : modulator(modulator) {
        constellation = modulator.getConstellation();
        bitsPerSym = modulator.getBitsPerSymbol();
    }
    
    std::vector<int> demodulate(const std::vector<std::complex<double>>& received) {
        std::vector<int> bits;
        for (const auto& sym : received) {

            int bestIndex = -1;
            double minDist = std::numeric_limits<double>::max();
            
            for (const auto& [idx, point] : constellation) {
                double dist = std::norm(sym - point);
                if (dist < minDist) {
                    minDist = dist;
                    bestIndex = idx;
                }
            }
            
            for (int i = bitsPerSym - 1; i >= 0; --i) {
                bits.push_back((bestIndex >> i) & 1);
            }
        }
        return bits;
    }
    
private:
    const QAMModulator& modulator;
    std::map<int, std::complex<double>> constellation;
    int bitsPerSym;
};

double calculateBER(const std::vector<int>& original, const std::vector<int>& demodulated) {
    if (original.size() != demodulated.size()) {
        throw std::runtime_error("Несоответствие размеров");
    }
    int errors = 0;
    for (size_t i = 0; i < original.size(); ++i) {
        if (original[i] != demodulated[i]) {
            errors++;
        }
    }
    return static_cast<double>(errors) / original.size();
}

std::vector<int> generateRandomBits(int numBits) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 1);
    
    std::vector<int> bits(numBits);
    for (int i = 0; i < numBits; ++i) {
        bits[i] = dis(gen);
    }
    return bits;
}

void runSimulation(QAMModulator::Type modType, 
                   const std::vector<double>& variances,
                   int numBits,
                   const std::string& filename) {
    
    std::string modName;
    switch(modType) {
        case QAMModulator::QPSK: modName = "QPSK"; break;
        case QAMModulator::QAM16: modName = "QAM16"; break;
        case QAMModulator::QAM64: modName = "QAM64"; break;
    }
    
    QAMModulator modulator(modType);
    GAUSSChannel channel;
    QAMDemodulator demodulator(modulator);
    
    std::vector<int> originalBits = generateRandomBits(numBits);
    
    std::ofstream outFile(filename);
    outFile << "# " << modName << " - BER vs Noise Variance\n";
    outFile << "# Variance\tBER\n";
    
    std::cout << "Simulating " << modName << " with " << numBits << " bits...\n";
    
    for (double var : variances) {
        channel.setVariance(var);
        
        auto symbols = modulator.modulate(originalBits);
        
        auto noisySymbols = channel.addNoise(symbols);
        
        auto demodulatedBits = demodulator.demodulate(noisySymbols);
        
        double ber = calculateBER(originalBits, demodulatedBits);
        
        outFile << std::scientific << std::setprecision(6) << var << "\t" << ber << "\n";
        std::cout << "  Variance = " << var << " -> BER = " << ber << "\n";
    }
    
    outFile.close();
    std::cout << "Results saved to " << filename << "\n\n";
}

int main() {

    const int numBits = 200000; 
    
    std::vector<double> variances;
    for (double snr_db = 20; snr_db >= 0; snr_db -= 1.0) {
        double var = 1.0 / pow(10.0, snr_db / 10.0);
        variances.push_back(var);
    }
    
    runSimulation(QAMModulator::QPSK, variances, numBits, "ber_qpsk.txt");
    runSimulation(QAMModulator::QAM16, variances, numBits, "ber_qam16.txt");
    runSimulation(QAMModulator::QAM64, variances, numBits, "ber_qam64.txt");
    
    std::cout << R"(import numpy as np
import matplotlib.pyplot as plt

qpsk = np.loadtxt('ber_qpsk.txt', comments='#')
qam16 = np.loadtxt('ber_qam16.txt', comments='#')

qpsk = qpsk[qpsk[:,1] > 0]

snr_qpsk = 10 * np.log10(1.0 / qpsk[:,0])
snr_qam16 = 10 * np.log10(1.0 / qam16[:,0])

plt.figure(figsize=(10, 6))
plt.semilogy(snr_qpsk, qpsk[:,1], 'o-', label='QPSK')
plt.semilogy(snr_qam16, qam16[:,1], 's-', label='16-QAM')
plt.xlabel('SNR (dB)')
plt.ylabel('Bit Error Rate (BER)')
plt.title('BER и SNR для QAM Модуляции')
plt.grid(True, which='both', linestyle='--', alpha=0.7)
plt.legend()
plt.ylim([1e-6, 1])
plt.show())";
    
    return 0;
}