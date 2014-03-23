/* LD decoder prototype, Copyright (C) 2013 Chad Page.  License: LGPL2 */

#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <list>
#include <queue>
#include <complex>
#include <unistd.h>
#include <sys/fcntl.h>
#include <fftw3.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// capture frequency and fundamental NTSC color frequency
const double CHZ = (1000000.0*(315.0/88.0)*8.0);

using namespace std;

double ctor(double r, double i)
{
	return sqrt((r * r) + (i * i));
}

inline double dftc(double *buf, int offset, int len, double bin, double &fc, double &fci) 
{
	fc = 0.0; fci = 0.0;

	for (int k = (-len + 1); k < len; k++) {
//		cout << offset + k << ' ' << len << endl;
		double o = buf[offset + k]; 
		
		fc += (o * cos((2.0 * M_PIl * ((double)(offset - k) / bin)))); 
		fci -= (o * sin((2.0 * M_PIl * ((double)(offset - k) / bin)))); 
	}

	return ctor(fc, fci);
}

inline double dft(double *buf, int offset, int len, double bin) 
{
	double fc, fci;

	return dftc(buf, offset, len, bin, fc, fci);
}

class Filter {
	protected:
		int order;
		bool isIIR;
		vector<double> a, b;
		vector<double> y, x;
	public:
		Filter(int _order, const double *_a, const double *_b) {
			order = _order + 1;
			if (_a) {
				a.insert(b.begin(), _a, _a + order);
				isIIR = true;
			} else {
				a.push_back(1.0);
				isIIR = false;
			}
			b.insert(b.begin(), _b, _b + order);
			x.resize(order);
			y.resize(order);
	
			clear();
		}

		Filter(Filter *orig) {
			order = orig->order;
			isIIR = orig->isIIR;
			a = orig->a;
			b = orig->b;
			x.resize(order);
			y.resize(order);
				
			clear();
		}

		void clear(double val = 0) {
			for (int i = 0; i < order; i++) {
				x[i] = y[i] = val;
			}
		}

		inline double feed(double val) {
			double a0 = a[0];
			double y0;

			double *x_data = x.data();
			double *y_data = y.data();

			memmove(&x_data[1], x_data, sizeof(double) * (order - 1)); 
			if (isIIR) memmove(&y_data[1], y_data, sizeof(double) * (order - 1)); 

			x[0] = val;
			y0 = 0; // ((b[0] / a0) * x[0]);
			//cerr << "0 " << x[0] << ' ' << b[0] << ' ' << (b[0] * x[0]) << ' ' << y[0] << endl;
			if (isIIR) {
				for (int o = 0; o < order; o++) {
					y0 += ((b[o] / a0) * x[o]);
					if (o) y0 -= ((a[o] / a0) * y[o]);
					//cerr << o << ' ' << x[o] << ' ' << y[o] << ' ' << a[o] << ' ' << b[o] << ' ' << (b[o] * x[o]) << ' ' << -(a[o] * y[o]) << ' ' << y[0] << endl;
				}
			} else {
				if (order == 13) {
					double t[4];
		
					// Cycling through destinations reduces pipeline stalls.	
					t[0] = b[0] * x[0];
					t[1] = b[1] * x[1];
					t[2] = b[2] * x[2];
					t[3] = b[3] * x[3];
					t[0] += b[4] * x[4];
					t[1] += b[5] * x[5];
					t[2] += b[6] * x[6];
					t[3] += b[7] * x[7];
					t[0] += b[8] * x[8];
					t[1] += b[9] * x[9];
					t[2] += b[10] * x[10];
					t[3] += b[11] * x[11];
					y0 = t[0] + t[1] + t[2] + t[3] + (b[12] * x[12]);
				} else for (int o = 0; o < order; o++) {
					y0 += b[o] * x[o];
				}
			}

			y[0] = y0;
			return y[0];
		}
		double val() {return y[0];}
};

// b = fir2(8, [0, 3.0/freq, 3.5/freq, 4.0/freq, 5/freq, 7/freq, 9/freq, 11/freq, 13/freq, 1], [0.0, 0.0, 0.5, 1.0, 1.2, 1.6, 2.0, 2.4, 2.6, 2.6] 
//const double f_boost6_b[] {-4.033954487174667e-03, -3.408583476980324e-02, -5.031202829325306e-01, 1.454592400360107e+00, -5.031202829325309e-01, -3.408583476980324e-02, -4.033954487174666e-03};
//const double f_boost8_b[] {1.990859784029516e-03, -1.466569224478291e-02, -3.522213674516057e-02, -6.922384231866260e-01, 1.669825180053711e+00, -6.922384231866261e-01, -3.522213674516058e-02, -1.466569224478292e-02, 1.990859784029516e-03};


// b = fir2(12, [0 .18 .22 .5 1], [0 0 1 2 3], 'hamming'); freqz(b)
const double f_boost12_b[] {2.963585204586335e-03, 8.021303205163649e-04, -8.167321049713539e-04, -5.586785422763135e-02, -1.209392722622762e-01, -6.272452360792947e-01, 1.689996991838728e+00, -6.272452360792948e-01, -1.209392722622763e-01, -5.586785422763134e-02, -8.167321049713538e-04, 8.021303205163657e-04, 2.963585204586334e-03};

//const double f_boost16_b[] {3.023991564221081e-03, 4.233186409767337e-03, 7.954665760931824e-03, 2.061366484849445e-03, -1.422694634466230e-03, -7.408019315126677e-02, -1.359026202658482e-01, -6.450343643150648e-01, 1.689996991838728e+00, -6.450343643150648e-01, -1.359026202658483e-01, -7.408019315126678e-02, -1.422694634466230e-03, 2.061366484849445e-03, 7.954665760931824e-03, 4.233186409767340e-03, 3.023991564221081e-03};

// b = fir2(16, [0 .15 .28 .30 .5 .75 1], [.5 0 0 2 2.5 3 2], 'hamming'); freqz(b)
const double f_boost16_b[] {-4.335748575458251e-03, -2.388405917037859e-05, 1.649789644901516e-02, 6.613559160825309e-02, 3.064480899148049e-02, 4.802540855089615e-02, -3.181748983230354e-01, -6.172100703119979e-01, 1.820000330607096e+00, -6.172100703119979e-01, -3.181748983230355e-01, 4.802540855089617e-02, 3.064480899148050e-02, 6.613559160825308e-02, 1.649789644901517e-02, -2.388405917037859e-05, -4.335748575458251e-03};

// b = fir2(32, [0 (7.6/freq) (9.3/freq) 1], [0 1 1 0]); freqz(b)
//const double f_boost32_b[] {-1.187938784850855e-04, 1.924864474237211e-04, -4.682408675527418e-04, -3.036656640089152e-05, -2.585494098412473e-04, 1.289575082757557e-05, -2.125999321229957e-03, 1.156326652444841e-03, -4.889041225678126e-03, 2.256846453819866e-03, -7.270505707623612e-03, -6.252912087972349e-03, -2.114076458620643e-02, 4.491066604892546e-02, -2.021489313184016e-01, -5.063734983445588e-02, 5.593630925815964e-01, -5.063734983445588e-02, -2.021489313184016e-01, 4.491066604892546e-02, -2.114076458620643e-02, -6.252912087972349e-03, -7.270505707623615e-03, 2.256846453819867e-03, -4.889041225678127e-03, 1.156326652444841e-03, -2.125999321229957e-03, 1.289575082757559e-05, -2.585494098412474e-04, -3.036656640089140e-05, -4.682408675527423e-04, 1.924864474237212e-04, -1.187938784850855e-04};

// b = fir2(32, [0 (2.0/freq) (7.6/freq) (9.3/freq) (12.0/freq) 1], [0 0 1 1 0 0]); freqz(b)
//const double f_boost32_b[] {4.476472917788309e-05, 1.976225899668346e-04, -6.959206992855477e-04, 1.301322776632257e-04, -1.980840709750101e-04, 3.040872700850747e-04, -9.200842912615518e-04, 2.135210152131823e-03, 2.622246176400500e-03, -1.910303590308619e-03, 2.096925422998692e-02, -2.532983779683615e-02, 1.972856357369315e-02, 5.774390801105213e-02, -2.459093215932984e-01, -3.316625612256505e-02, 4.085702249326060e-01, -3.316625612256504e-02, -2.459093215932984e-01, 5.774390801105213e-02, 1.972856357369316e-02, -2.532983779683615e-02, 2.096925422998693e-02, -1.910303590308620e-03, 2.622246176400499e-03, 2.135210152131824e-03, -9.200842912615518e-04, 3.040872700850751e-04, -1.980840709750101e-04, 1.301322776632260e-04, -6.959206992855478e-04, 1.976225899668347e-04, 4.476472917788309e-05};

// b = fir2(32, [0 (2.0/freq) (7.6/freq) (9.3/freq) (11.0/freq) 1], [0 0 1 1 0 0]); freqz(b)
//const double f_boost32_b[] { -1.316278648737407e-04, 2.114433400397588e-04, -3.022480871174253e-04, -1.086838450974835e-03, 1.949383454729347e-03, -1.647868984155162e-03, -1.873252636148228e-03, 8.662850151764790e-03, -8.728525178941121e-03, 7.913451980478978e-03, 2.201095814626406e-02, -4.221539150684461e-02, 4.648637563149476e-02, 3.689858368315863e-02, -2.463304990404538e-01, -8.724693819945922e-03, 3.736508080210734e-01, -8.724693819945921e-03, -2.463304990404538e-01, 3.689858368315863e-02, 4.648637563149476e-02, -4.221539150684461e-02, 2.201095814626407e-02, 7.913451980478979e-03, -8.728525178941121e-03, 8.662850151764790e-03, -1.873252636148227e-03, -1.647868984155163e-03, 1.949383454729348e-03, -1.086838450974836e-03, -3.022480871174252e-04, 2.114433400397589e-04, -1.316278648737407e-04 };

const double f_boost16ls_b[] {-3.639859302988038e-02, -2.699256214524854e-02, 7.330157275880748e-02, 4.830585618088179e-02, 5.101906550092963e-02, -2.809752571172393e-02, -3.432858590790925e-01, 3.800130420612720e-03, 5.089363319596238e-01, 3.800130420612720e-03, -3.432858590790925e-01, -2.809752571172393e-02, 5.101906550092963e-02, 4.830585618088179e-02, 7.330157275880748e-02, -2.699256214524854e-02, -3.639859302988038e-02};

// b = fir2(32, [0 (2.0/freq) (7.6/freq) (9.3/freq) (11.0/freq) 1], [0 0 1 1 0 0]); freqz(b)
const double f_boost32_b[] {1.054426894146890e-04, -4.855229756583843e-05, -1.697044474992538e-04, -7.766136246382485e-04, 9.144665108615849e-04, -1.491605732025549e-04, -2.685488739297526e-03, 7.285040311086869e-03, -4.774190752742531e-03, 3.330240008284701e-03, 2.358989562928025e-02, -3.821800878599309e-02, 3.820884674542058e-02, 4.425991853422013e-02, -2.472175319907102e-01, -1.569521671065990e-02, 3.841248896214869e-01, -1.569521671065990e-02, -2.472175319907102e-01, 4.425991853422012e-02, 3.820884674542059e-02, -3.821800878599308e-02, 2.358989562928026e-02, 3.330240008284701e-03, -4.774190752742532e-03, 7.285040311086868e-03, -2.685488739297526e-03, -1.491605732025550e-04, 9.144665108615855e-04, -7.766136246382485e-04, -1.697044474992539e-04, -4.855229756583846e-05, 1.054426894146890e-04};

// b = firls(32, [0 (2.0/freq) (3.0/freq) (4.0/freq) (7.6/freq) (9.3/freq) (12.0/freq) 1], [0 0 0 .5 1 1 0 0]); freqz(b)
const double f_boost32ls_b[] {-6.331860659456277e-03, 1.094803376080349e-03, 1.748658403710662e-02, 7.500770265936007e-03, 3.878695050769085e-04, -2.266291707900163e-02, -3.553391875243375e-02, 2.510421286790458e-02, 1.161628806695472e-02, -6.664172402822139e-03, 3.834832178435898e-02, -2.903851569459353e-02, 4.885655711441563e-02, 5.594904509378985e-02, -2.800136688809418e-01, -3.064597645193942e-02, 4.110568401320394e-01, -3.064597645193942e-02, -2.800136688809418e-01, 5.594904509378985e-02, 4.885655711441563e-02, -2.903851569459353e-02, 3.834832178435898e-02, -6.664172402822139e-03, 1.161628806695472e-02, 2.510421286790458e-02, -3.553391875243375e-02, -2.266291707900163e-02, 3.878695050769085e-04, 7.500770265936007e-03, 1.748658403710662e-02, 1.094803376080349e-03, -6.331860659456277e-03  };

// b = fir2(40, [0 (2.3/freq) (7.5/freq) (10/freq) (12.5/freq) 1], [0 -.07 1 1 0 0]); freqz(b)
//const double f_boost40_b[] {1.969570200127439e-04, 1.070259575741876e-04, -6.029510960417574e-05, 4.641253002586967e-04, -5.538087999089407e-04, 7.780668354704421e-04, -4.795250406891805e-04, -1.625239787091907e-03, 1.797211655303214e-04, 5.339384164946336e-04, -2.605178262397195e-03, -1.850890051873016e-03, 1.323605308417584e-02, -8.869976942696771e-03, 2.297250345967587e-02, -1.344199967696921e-03, 1.692454398977061e-03, 9.051430416185872e-02, -2.501110718273827e-01, -8.165751135257637e-02, 4.251565800571412e-01, -8.165751135257637e-02, -2.501110718273827e-01, 9.051430416185874e-02, 1.692454398977061e-03, -1.344199967696921e-03, 2.297250345967587e-02, -8.869976942696773e-03, 1.323605308417584e-02, -1.850890051873016e-03, -2.605178262397196e-03, 5.339384164946336e-04, 1.797211655303213e-04, -1.625239787091907e-03, -4.795250406891791e-04, 7.780668354704424e-04, -5.538087999089409e-04, 4.641253002586969e-04, -6.029510960417600e-05, 1.070259575741877e-04, 1.969570200127439e-04};

//  b = fir2(40, [0 (2.3/freq) (7.5/freq) (10/freq) (12.5/freq) 1], [0 -.07 1 1.5 0.05 0]); freqz(b)
//const double f_boost40_b[] {2.997827511273736e-04, -2.386292347710737e-05, -1.349808163787998e-04, 9.231818400775012e-04, -1.102256886220982e-03, 8.078637952131099e-04, 1.263829727497591e-05, -1.925969887997556e-03, 8.025265679197658e-05, 7.496660191206186e-04, -3.854163777549969e-03, 9.075760225101020e-04, 1.420235838053318e-02, -2.157106064636445e-02, 4.186284470607415e-02, 8.037423141559036e-05, -4.112557443673911e-02, 1.520382288901734e-01, -2.704038961052398e-01, -1.377003157655269e-01, 5.199962418287422e-01, -1.377003157655269e-01, -2.704038961052398e-01, 1.520382288901734e-01, -4.112557443673912e-02, 8.037423141559021e-05, 4.186284470607415e-02, -2.157106064636446e-02, 1.420235838053318e-02, 9.075760225101021e-04, -3.854163777549969e-03, 7.496660191206188e-04, 8.025265679197602e-05, -1.925969887997555e-03, 1.263829727497720e-05, 8.078637952131101e-04, -1.102256886220983e-03, 9.231818400775014e-04, -1.349808163787997e-04, -2.386292347710725e-05, 2.997827511273737e-04};

//const double f_boost40_b[] {3.055591284961469e-04, -2.357106928306128e-04, -2.579181918893931e-04, 1.412627751485582e-03, -1.465258595683867e-03, 1.203211936040546e-03, 9.556021957624761e-04, -1.831516494143334e-03, 1.631055487848852e-04, 6.123674973499082e-04, -6.339619974002717e-03, 1.734375074038977e-03, 1.324558783447752e-02, -3.575750820121307e-02, 6.010743451427301e-02, 3.431371225140131e-03, -7.756019704539031e-02, 2.220382503770410e-01, -2.874816887976011e-01, -2.006792688442605e-01, 6.026136536911959e-01, -2.006792688442605e-01, -2.874816887976011e-01, 2.220382503770411e-01, -7.756019704539034e-02, 3.431371225140131e-03, 6.010743451427300e-02, -3.575750820121307e-02, 1.324558783447752e-02, 1.734375074038977e-03, -6.339619974002718e-03, 6.123674973499082e-04, 1.631055487848841e-04, -1.831516494143335e-03, 9.556021957624775e-04, 1.203211936040547e-03, -1.465258595683867e-03, 1.412627751485583e-03, -2.579181918893934e-04, -2.357106928306128e-04, 3.055591284961469e-04};

// b = fir2(40, [0 (2.85/freq) (7.5/freq) (10/freq) (12.5/freq) 1], [0 -.07 1.4 2 0.1 0]); freqz(b)
const double f_boost40_b[] {2.080744705878557e-04, -1.993972740681683e-04, -3.660839776063611e-04, 1.090504661431919e-03, -1.210527480824420e-03, 1.713338053941143e-03, 8.462133252500291e-04, -1.528661916918473e-03, 8.525594807452244e-04, 9.602741428731142e-04, -7.511546126144147e-03, -2.739727427780670e-03, 1.407142012207614e-02, -3.042953213824740e-02, 4.974328955521423e-02, 3.892014364209970e-03, -3.837652472115201e-02, 2.161935403401451e-01, -3.629140220891830e-01, -1.968324015350557e-01, 6.955020411806832e-01, -1.968324015350557e-01, -3.629140220891831e-01, 2.161935403401451e-01, -3.837652472115202e-02, 3.892014364209970e-03, 4.974328955521422e-02, -3.042953213824741e-02, 1.407142012207614e-02, -2.739727427780669e-03, -7.511546126144148e-03, 9.602741428731142e-04, 8.525594807452242e-04, -1.528661916918474e-03, 8.462133252500298e-04, 1.713338053941144e-03, -1.210527480824421e-03, 1.090504661431920e-03, -3.660839776063609e-04, -1.993972740681680e-04, 2.080744705878558e-04};

const double f_boost40ls_b[] {-1.957887625664747e-03, 1.230371148628347e-03, 3.466350286708624e-03, -6.889607754975954e-04, -8.369973665907265e-04, -1.052603484559785e-02, 4.565007899027511e-03, 1.788161112872995e-02, -6.450936210645790e-03, -4.574541099546828e-03, -2.785940651177577e-02, 1.134907827884636e-02, 5.575414427060520e-02, -3.254360182059707e-02, -1.472218157242808e-02, -4.708545869821743e-02, 2.963960965074667e-02, 1.780492256531542e-01, -2.136855265232617e-01, -1.131322003311992e-01, 3.440787047842241e-01, -1.131322003311992e-01, -2.136855265232617e-01, 1.780492256531542e-01, 2.963960965074667e-02, -4.708545869821743e-02, -1.472218157242808e-02, -3.254360182059707e-02, 5.575414427060520e-02, 1.134907827884636e-02, -2.785940651177577e-02, -4.574541099546828e-03, -6.450936210645790e-03, 1.788161112872995e-02, 4.565007899027511e-03, -1.052603484559785e-02, -8.369973665907265e-04, -6.889607754975954e-04, 3.466350286708624e-03, 1.230371148628347e-03, -1.957887625664747e-03};

const double f_boost48_b[] {-2.503453409533724e-05, 2.347803325402812e-05, 5.035795234554012e-05, -1.830946337150403e-04, 3.486836340739025e-04, 2.187130172161023e-04, -1.262971074016088e-04, 9.271654941559823e-04, -1.022809029499816e-03, 1.318811612824120e-03, -7.486600932499714e-04, -2.355467041199974e-03, 2.439333843921380e-04, 6.845256840605290e-04, -3.179556197748905e-03, -2.165666424352164e-03, 1.494069263971964e-02, -9.713279102543259e-03, 2.452845333027145e-02, -1.405832822620792e-03, 1.741111755233682e-03, 9.195526947626964e-02, -2.518612046906074e-01, -8.179941917165474e-02, 4.251565800571412e-01, -8.179941917165474e-02, -2.518612046906074e-01, 9.195526947626965e-02, 1.741111755233682e-03, -1.405832822620792e-03, 2.452845333027146e-02, -9.713279102543260e-03, 1.494069263971965e-02, -2.165666424352164e-03, -3.179556197748905e-03, 6.845256840605288e-04, 2.439333843921378e-04, -2.355467041199973e-03, -7.486600932499695e-04, 1.318811612824121e-03, -1.022809029499816e-03, 9.271654941559836e-04, -1.262971074016094e-04, 2.187130172161026e-04, 3.486836340739028e-04, -1.830946337150402e-04, 5.035795234554019e-05, 2.347803325402812e-05, -2.503453409533716e-05};

// b = fir2(32, [0 (1.9/freq) (2.1/freq) (3.7/freq) (3.8/freq) 1], [1 0 0 0 1 1]); freqz(b)
//const double f_afilt32_b[] {-8.723882104673613e-04, 4.204179159281918e-04, 2.301789248515278e-03, 3.921655187438559e-03, 3.271815932898895e-03, -9.854589025634270e-04, -6.549893521013261e-03, -6.594987775754657e-03, 6.457014099766486e-03, 3.370519502482751e-02, 6.438109974712757e-02, 7.805157132749757e-02, 5.551621605005222e-02, -7.402574623736323e-03, -9.273971982202059e-02, -1.664337511088562e-01, 8.044751911072069e-01, -1.664337511088562e-01, -9.273971982202059e-02, -7.402574623736322e-03, 5.551621605005223e-02, 7.805157132749757e-02, 6.438109974712759e-02, 3.370519502482752e-02, 6.457014099766487e-03, -6.594987775754656e-03, -6.549893521013260e-03, -9.854589025634272e-04, 3.271815932898897e-03, 3.921655187438559e-03, 2.301789248515280e-03, 4.204179159281921e-04, -8.723882104673613e-04};

const double f_afilt32_b[] {9.201204713927718e-05, -8.215891101837309e-04, -1.962955231719582e-03, -2.920611799068991e-03, -2.424184203096596e-03, 1.325255529340073e-03, 9.632186032515058e-03, 2.201068206548717e-02, 3.535122191053103e-02, 4.422670876630320e-02, 4.254393864069367e-02, 2.606132957994222e-02, -5.262900241240934e-03, -4.623814370587761e-02, -8.741885772140195e-02, -1.179400823255169e-01, 8.707983302560083e-01, -1.179400823255169e-01, -8.741885772140195e-02, -4.623814370587761e-02, -5.262900241240934e-03, 2.606132957994221e-02, 4.254393864069369e-02, 4.422670876630321e-02, 3.535122191053103e-02, 2.201068206548717e-02, 9.632186032515056e-03, 1.325255529340075e-03, -2.424184203096597e-03, -2.920611799068991e-03, -1.962955231719584e-03, -8.215891101837312e-04, 9.201204713927718e-05};

//const double f_boost16_b[] {-0.0050930113529 , -0.00686822662698 , -0.00168241501333 , -0.00705635391054 , -0.0687556532642 , -0.13429140818 , -0.0399158340449 , 0.216750729476 , 0.363913859251 , 0.216750729476 , -0.0399158340449 , -0.13429140818 , -0.0687556532642 , -0.00705635391054 , -0.00168241501333 , -0.00686822662698 , -0.0050930113529 };

// b = fir2(16, [0 .15 .27 .32 .5 1], [.5 0 0 1 2 3],
//const double f_afilt16_b[] {-1.094921539437523e-03, -8.708333308919966e-04, 6.996462115002340e-03, 2.542163544295336e-02, 6.867782981853393e-02, 2.436746285806122e-02, -8.383418514411443e-02, -6.964586595832247e-01, 1.582504272460938e+00, -6.964586595832248e-01, -8.383418514411445e-02, 2.436746285806124e-02, 6.867782981853394e-02, 2.542163544295336e-02, 6.996462115002342e-03, -8.708333308919975e-04, -1.094921539437523e-03};

// b = fir2(16, [0 .15 .25 .30 .5 1], [.5 0 0 2 2.5 3], 'hamming');
const double f_afilt16_b[] {-2.519060855786711e-03, 3.091645751353658e-03, 2.196256697928431e-02, 4.864434495016245e-02, 5.651976618169693e-02, -4.487712453248293e-02, -2.348241207734272e-01, -6.725953023781852e-01, 1.912483851114909e+00, -6.725953023781851e-01, -2.348241207734273e-01, -4.487712453248295e-02, 5.651976618169696e-02, 4.864434495016244e-02, 2.196256697928432e-02, 3.091645751353661e-03, -2.519060855786711e-03};

const double f_afilt64_b[] {2.306569535976141e-04, 1.412745600306265e-04, -9.489350887102363e-05, -3.383020909817939e-04, -3.411685787720725e-04, 7.064802809220064e-05, 7.282908228862322e-04, 1.050221582812325e-03, 4.447141005216792e-04, -9.724872308468346e-04, -2.036014746587884e-03, -1.234216834451701e-03, 1.754673485839526e-03, 5.023803801281434e-03, 5.346818173498242e-03, 9.871014064655795e-04, -5.772337161542854e-03, -9.210166104536770e-03, -4.553789396303364e-03, 6.841484844520272e-03, 1.649725056711512e-02, 1.456858216192505e-02, -8.799231236364160e-04, -1.884544321017932e-02, -2.088038949594207e-02, 4.423410617719910e-03, 4.806665067590345e-02, 8.029845511509311e-02, 6.713690124938526e-02, -5.394502580060357e-03, -1.163645776662770e-01, -2.169909822893113e-01, 7.425028483072916e-01, -2.169909822893113e-01, -1.163645776662770e-01, -5.394502580060358e-03, 6.713690124938528e-02, 8.029845511509313e-02, 4.806665067590344e-02, 4.423410617719911e-03, -2.088038949594207e-02, -1.884544321017933e-02, -8.799231236364169e-04, 1.456858216192505e-02, 1.649725056711513e-02, 6.841484844520272e-03, -4.553789396303364e-03, -9.210166104536772e-03, -5.772337161542855e-03, 9.871014064655804e-04, 5.346818173498242e-03, 5.023803801281436e-03, 1.754673485839527e-03, -1.234216834451702e-03, -2.036014746587887e-03, -9.724872308468343e-04, 4.447141005216792e-04, 1.050221582812326e-03, 7.282908228862324e-04, 7.064802809220000e-05, -3.411685787720733e-04, -3.383020909817939e-04, -9.489350887102335e-05, 1.412745600306273e-04, 2.306569535976141e-04};

//const double f_boost8_b[] {8.188360043288829e-04, -1.959020553078470e-02, -6.802011723017314e-02, -4.157977388307656e-01, 1.209527775037999e+00, -4.157977388307657e-01, -6.802011723017315e-02, -1.959020553078471e-02, 8.188360043288829e-04};

const double f_boost8_b[] {-1.252993897181109e-03, -1.811981140446628e-02, -8.500709379119413e-02, -1.844252402264797e-01, 7.660358082164418e-01, -1.844252402264797e-01, -8.500709379119414e-02, -1.811981140446629e-02, -1.252993897181109e-03};

const double f_lpf525_12_hamming_b[] {2.416267218983970e-03, -4.599440255094788e-03, -2.435276138108525e-02, -1.709969522380537e-02, 9.102385774622326e-02, 2.708622944399880e-01, 3.634989549095802e-01, 2.708622944399882e-01, 9.102385774622331e-02, -1.709969522380538e-02, -2.435276138108525e-02, -4.599440255094792e-03, 2.416267218983970e-03};

const double f_lpf49_8_b[] {-6.035564708478322e-03, -1.459747550010019e-03, 7.617213234063192e-02, 2.530939844348266e-01, 3.564583909660596e-01, 2.530939844348267e-01, 7.617213234063196e-02, -1.459747550010020e-03, -6.035564708478321e-03};

const double f_lpf45_8_b[] {9.550931633601412e-19, 1.601492907105197e-03, 6.040483227758160e-02, 2.483137482510164e-01, 3.793598531285934e-01, 2.483137482510165e-01, 6.040483227758162e-02, 1.601492907105199e-03, 9.550931633601412e-19};

const double f_lpf45_12_hamming_b[] {-1.560564704684075e-03, -8.799707436385511e-03, -1.757949972644727e-02, 1.072420923958327e-02, 1.127204763471358e-01, 2.482016652603697e-01, 3.125868420408562e-01, 2.482016652603697e-01, 1.127204763471359e-01, 1.072420923958327e-02, -1.757949972644727e-02, -8.799707436385517e-03, -1.560564704684075e-03  };

// freq = freq =4.0*(315.0/88.0)
// fir1(12, (4.2/freq), 'hamming');
const double f_lpf42_12_hamming_b[] {-2.968012952158944e-03, -8.970442103421515e-03, -1.254603780275414e-02, 2.162767371309263e-02, 1.184891740848597e-01, 2.378741316708058e-01, 2.929870267791529e-01, 2.378741316708059e-01, 1.184891740848597e-01, 2.162767371309263e-02, -1.254603780275414e-02, -8.970442103421522e-03, -2.968012952158944e-03}; 

const double f_lpf30_16_hamming_b[] {-2.764895502720406e-03, -5.220462214367938e-03, -8.137721102693703e-03, -3.120835066368537e-03, 2.151916440426718e-02, 7.057010452167467e-02, 1.339005076970342e-01, 1.883266182415400e-01, 2.098550380432692e-01, 1.883266182415399e-01, 1.339005076970343e-01, 7.057010452167471e-02, 2.151916440426718e-02, -3.120835066368536e-03, -8.137721102693705e-03, -5.220462214367943e-03, -2.764895502720406e-03};

const double f_lpf35_16_hamming_b[] {-5.182956535966573e-04, -4.174028437151462e-03, -1.126381254549101e-02, -1.456598548706209e-02, 3.510439201231994e-03, 5.671595743858979e-02, 1.370914830220347e-01, 2.119161192395519e-01, 2.425762464437853e-01, 2.119161192395519e-01, 1.370914830220347e-01, 5.671595743858982e-02, 3.510439201231995e-03, -1.456598548706209e-02, -1.126381254549101e-02, -4.174028437151466e-03, -5.182956535966573e-04};

const double f_lpf30_16_python_b[] {-0.00272340916057 , -0.00523141288763 , -0.00828142266599 , -0.00347333316109 , 0.0210474458802 , 0.0702625825631 , 0.134050691125 , 0.188983003025 , 0.210731710564 , 0.188983003025 , 0.134050691125 , 0.0702625825631 , 0.0210474458802 , -0.00347333316109 , -0.00828142266599 , -0.00523141288763 , -0.00272340916057};

const double f_lpf35_16_python_b[] {-0.000441330317833 , -0.00410580778703 , -0.0112866761199 , -0.0148376907459 , 0.00298625401005 , 0.0562463748607 , 0.137108704283 , 0.212569087382 , 0.243522168871 , 0.212569087382 , 0.137108704283 , 0.0562463748607 , 0.00298625401005 , -0.0148376907459 , -0.0112866761199 , -0.00410580778703 , -0.000441330317833};

const double f_lpf38_16_python_b[] {0.0011951808488 , -0.00224845294632 , -0.0108604095518 , -0.0197314649541 , -0.00822086688087 , 0.0451598012206 , 0.136377157504 , 0.22629941124 , 0.264059287039 , 0.22629941124 , 0.136377157504 , 0.0451598012206 , -0.00822086688087 , -0.0197314649541 , -0.0108604095518 , -0.00224845294632 , 0.0011951808488};

const double f_lpf40_16_python_b[] {0.00213061018499 , -0.000724124441657 , -0.00964071850941 , -0.0218506974226 , -0.0154196049218 , 0.0368070919822 , 0.134674858501 , 0.235061918569 , 0.277921332118 , 0.235061918569 , 0.134674858501 , 0.0368070919822 , -0.0154196049218 , -0.0218506974226 , -0.00964071850941 , -0.000724124441657 , 0.00213061018499};

const double f_lpf42_16_python_b[] {0.00280667642657 , 0.000867823733568 , -0.00775899444297 , -0.0229278618145 , -0.0221485357312 , 0.0278269912518 , 0.131971347604 , 0.243434045133 , 0.29185701568 , 0.243434045133 , 0.131971347604 , 0.0278269912518 , -0.0221485357312 , -0.0229278618145 , -0.00775899444297 , 0.000867823733568 , 0.00280667642657};

const double f_lpf45_16_python_b[] {0.0031653903905 , 0.00306014145217 , -0.00398454468472 , -0.0224868006252 , -0.0309181593988 , 0.013503739459 , 0.12605232633 , 0.25518176899 , 0.312852276173 , 0.25518176899 , 0.12605232633 , 0.013503739459 , -0.0309181593988 , -0.0224868006252 , -0.00398454468472 , 0.00306014145217 , 0.0031653903905};

const double f_lpf50_16_python_b[] {0.00191607102022 , 0.00513481488446 , 0.0033474955952 , -0.0165362843732 , -0.0406091727117 , -0.0112885298755 , 0.111470359277 , 0.272497891277 , 0.348134709814 , 0.272497891277 , 0.111470359277 , -0.0112885298755 , -0.0406091727117 , -0.0165362843732 , 0.0033474955952 , 0.00513481488446 , 0.00191607102022};

const double f_lpf50_14_python_b[] {3.563789088660302e-03 , 1.953036852592052e-03 , -1.147850770946271e-02 , -3.283983947607802e-02 , -1.010496974431545e-02 , 1.062832238789432e-01 , 2.689349543936171e-01 , 3.473766254320870e-01 , 2.689349543936171e-01 , 1.062832238789433e-01 , -1.010496974431545e-02 , -3.283983947607806e-02 , -1.147850770946272e-02 , 1.953036852592050e-03 , 3.563789088660302e-03};

const double f_lpf50_32_python_b[] {-0.00153514027372 , -0.00128484804517 , 0.000896191796755 , 0.00383478453322 , 0.00321506486168 , -0.0039443397662 , -0.0116050394341 , -0.00692331358262 , 0.0129993404531 , 0.0282577143598 , 0.011219288771 , -0.0363293568899 , -0.0654014729708 , -0.0146172118449 , 0.124949497855 , 0.281315083295 , 0.349907513762 , 0.281315083295 , 0.124949497855 , -0.0146172118449 , -0.0654014729708 , -0.0363293568899 , 0.011219288771 , 0.0282577143598 , 0.0129993404531 , -0.00692331358262 , -0.0116050394341 , -0.0039443397662 , 0.00321506486168 , 0.00383478453322 , 0.000896191796755 , -0.00128484804517 , -0.00153514027372 };

const double f_lpf55_16_python_b[] {-0.000723397637219 , 0.00433368634435 , 0.00931049560886 , -0.00571459940902 , -0.0426674090828 , -0.0349785521301 , 0.0915883051498 , 0.286887403184 , 0.383928135944 , 0.286887403184 , 0.0915883051498 , -0.0349785521301 , -0.0426674090828 , -0.00571459940902 , 0.00931049560886 , 0.00433368634435 , -0.000723397637219};

const double f_lpf40_16_hamming_b[] {2.072595013361582e-03, -8.346396795579358e-04, -9.749056644931597e-03, -2.173598335596238e-02, -1.492934693656081e-02, 3.741335236370385e-02, 1.348268127802617e-01, 2.344615998458949e-01, 2.769493332275816e-01, 2.344615998458949e-01, 1.348268127802617e-01, 3.741335236370387e-02, -1.492934693656081e-02, -2.173598335596238e-02, -9.749056644931598e-03, -8.346396795579367e-04, 2.072595013361582e-03};
//const double f_lpf40_16_hamming_b[] {+0.0009957268915790, -0.0031904586206958, -0.0132827104419253, -0.0202383155846346, +0.0037119767122397, +0.0791997228694475, +0.1863911336253358, +0.2664129245486536, +0.2664129245486536, +0.1863911336253359, +0.0791997228694475, +0.0037119767122397, -0.0202383155846346, -0.0132827104419253, -0.0031904586206958, +0.0009957268915790};
//const double f_lpf30_16_hamming_b[] {-0.0027232929766121, -0.0052314380584097, -0.0082818086836746, -0.0034742916052344, +0.0210461556331407, +0.0702617355565447, +0.1340510944780587, +0.1889847932062197, +0.2107341048999342, +0.1889847932062197, +0.1340510944780587, +0.0702617355565447, +0.0210461556331407, -0.0034742916052344, -0.0082818086836746, -0.0052314380584097, -0.0027232929766121 };

const double f_lpf40_8_b[] {5.010487312257435e-19, 4.533965882743306e-03, 6.918575012753858e-02, 2.454450712419436e-01, 3.616704254955491e-01, 2.454450712419436e-01, 6.918575012753861e-02, 4.533965882743313e-03, 5.010487312257435e-19};

const double f_lpf30_8_b[] {-8.776697132906939e-19, 1.039295235883352e-02, 8.350051647243457e-02, 2.395856771132667e-01, 3.330417081109302e-01, 2.395856771132668e-01, 8.350051647243462e-02, 1.039295235883353e-02, -8.776697132906937e-19 };

const double f_lpf13_8_b[] {1.511108761398408e-02, 4.481461214778652e-02, 1.207230841165654e-01, 2.014075783203990e-01, 2.358872756025299e-01, 2.014075783203991e-01, 1.207230841165654e-01, 4.481461214778654e-02, 1.511108761398408e-02};

const double f_lpf06_8_b[] {-3.968132946649921e-18, 1.937504813888935e-02, 1.005269160761195e-01, 2.306204207693455e-01, 2.989552300312914e-01, 2.306204207693455e-01, 1.005269160761196e-01, 1.937504813888937e-02, -3.968132946649921e-18};

// allpass(16, 0, 4500000, .18, 28636363)
const double f_allpass_32_a[] {1.000000000000000e+00, -4.661913380623261e+00, 1.064710585646689e+01, -1.586434405195780e+01, 1.732760974789974e+01, -1.477833292685084e+01, 1.023735345653153e+01, -5.915510605579856e+00, 2.905871482191667e+00, -1.230567627146483e+00, 4.539790471091109e-01, -1.470684389054119e-01, 4.208842895460067e-02, -1.068797172802007e-02, 2.415921342991526e-03, -4.870790014993134e-04, 8.767422026987641e-05, -1.408965327232657e-05, 2.019564936217143e-06, -2.576737932141534e-07, 2.917239117680707e-08, -2.917651156698731e-09, 2.562406646490355e-10, -1.960487056801784e-11, 1.293078301449386e-12, -7.250455560811260e-14, 3.391158222648691e-15, -1.288268167152384e-16, 3.821507774727634e-18, -8.309773947720257e-20, 1.178872530133606e-21, -8.193088729422592e-24};
const double f_allpass_32_b[] {-8.193088729422592e-24, 1.178872530133606e-21, -8.309773947720258e-20, 3.821507774727635e-18, -1.288268167152384e-16, 3.391158222648691e-15, -7.250455560811263e-14, 1.293078301449386e-12, -1.960487056801785e-11, 2.562406646490355e-10, -2.917651156698731e-09, 2.917239117680706e-08, -2.576737932141534e-07, 2.019564936217142e-06, -1.408965327232657e-05, 8.767422026987638e-05, -4.870790014993133e-04, 2.415921342991524e-03, -1.068797172802007e-02, 4.208842895460066e-02, -1.470684389054119e-01, 4.539790471091108e-01, -1.230567627146483e+00, 2.905871482191666e+00, -5.915510605579854e+00, 1.023735345653153e+01, -1.477833292685084e+01, 1.732760974789974e+01, -1.586434405195780e+01, 1.064710585646689e+01, -4.661913380623261e+00, 1.000000000000000e+00};

// [n, Wc] = buttord((4.2 / freq), (5.0 / freq), 3, 20); [b, a] = butter(n, Wc)
const double f_lpf42b_6_a[] {1.000000000000000e+00, -1.725766598897363e+00, 1.442154506105485e+00, -5.692339148539284e-01, 9.129202080332011e-02};
const double f_lpf42b_6_b[] {1.490287582234461e-02, 5.961150328937842e-02, 8.941725493406763e-02, 5.961150328937842e-02, 1.490287582234461e-02};

const double f_lpf42b_3_a[] {1.000000000000000e+00, -1.302684590787800e+00, 7.909829879855602e-01, -1.641975612274331e-01};
const double f_lpf42b_3_b[] {4.051260449629090e-02, 1.215378134888727e-01, 1.215378134888727e-01, 4.051260449629090e-02  };
 

Filter f_lpf(16, NULL, f_lpf50_16_python_b);

Filter f_allpass(31, f_allpass_32_a, f_allpass_32_b);

// From http://lists.apple.com/archives/perfoptimization-dev/2005/Jan/msg00051.html.  Used w/o permission, but will replace when
// going integer... probably!
const double PI_FLOAT = M_PIl;
const double PIBY2_FLOAT = (M_PIl/2.0); 
// |error| < 0.005
double fast_atan2( double y, double x )
{
	if ( x == 0.0f )
	{
		if ( y > 0.0f ) return PIBY2_FLOAT;
		if ( y == 0.0f ) return 0.0f;
		return -PIBY2_FLOAT;
	}
	double atan;
	double z = y/x;
	if (  fabs( z ) < 1.0f  )
	{
		atan = z/(1.0f + 0.28f*z*z);
		if ( x < 0.0f )
		{
			if ( y < 0.0f ) return atan - PI_FLOAT;
			return atan + PI_FLOAT;
		}
	}
	else
	{
		atan = PIBY2_FLOAT - z/(z*z + 0.28f);
		if ( y < 0.0f ) return atan - PI_FLOAT;
	}
	return atan;
}


typedef vector<complex<double>> v_cossin;

class FM_demod {
	protected:
		double ilf;
		vector<Filter> f_q, f_i;
		vector<Filter *> f_pre;
		Filter *f_post;

		vector<v_cossin> ldft;
		double avglevel[40];

		double cbuf[9];
	
		int linelen;

		int min_offset;

		double deemp;

		vector<double> fb;
	public:
		FM_demod(int _linelen, vector<double> _fb, vector<Filter *> prefilt, vector<Filter *> filt, Filter *postfilt) {
			int i = 0;
			linelen = _linelen;

			fb = _fb;

			ilf = 8600000;
			deemp = 0;

			for (double f : fb) {
				v_cossin tmpdft;
				double fmult = f / CHZ; 

				for (int i = 0; i < linelen; i++) {
					tmpdft.push_back(complex<double>(sin(i * 2.0 * M_PIl * fmult), cos(i * 2.0 * M_PIl * fmult))); 
					// cerr << sin(i * 2.0 * M_PIl * fmult) << ' ' << cos(i * 2.0 * M_PIl * fmult) << endl;
				}	
				ldft.push_back(tmpdft);

				f_i.push_back(Filter(filt[i]));
				f_q.push_back(Filter(filt[i]));

				i++;
			}

			f_pre = prefilt;
			f_post = postfilt ? new Filter(*postfilt) : NULL;

			for (int i = 0; i < 40; i++) avglevel[i] = 30;
			for (int i = 0; i < 9; i++) cbuf[i] = 8100000;

			min_offset = 128;
		}

		~FM_demod() {
//			if (f_pre) free(f_pre);
			if (f_post) free(f_post);
		}

		vector<double> process(vector<double> in) 
		{
			vector<double> out;
			vector<double> phase(fb.size() + 1);
			vector<double> level(fb.size() + 1);
			double avg = 0, total = 0.0;
			
			for (int i = 0; i < 9; i++) cbuf[i] = 8100000;

			if (in.size() < (size_t)linelen) return out;

			for (double n : in) avg += n / in.size();
			//cerr << avg << endl;

			int i = 0;
			for (double n : in) {
				vector<double> angle(fb.size() + 1);
				double peak = 500000, pf = 0.0;
				int npeak;
				int j = 0;

			//	n -= avg;
				total += fabs(n);
				for (Filter *f: f_pre) {
					n = f->feed(n);
				}

				angle[j] = 0;

//				cerr << i << ' ';
	
				for (double f: fb) {
					double fci = f_i[j].feed(n * ldft[j][i].real());
					double fcq = f_q[j].feed(-n * ldft[j][i].imag());
					double at2 = fast_atan2(fci, fcq);	
	
//					cerr << n << ' ' << fci << ' ' << fcq << ' ' ;

					level[j] = ctor(fci, fcq);
	
					angle[j] = at2 - phase[j];
					if (angle[j] > M_PIl) angle[j] -= (2 * M_PIl);
					else if (angle[j] < -M_PIl) angle[j] += (2 * M_PIl);
					
//					cerr << at2 << ' ' << angle[j] << ' '; 
				//	cerr << angle[j] << ' ';
						
					if (fabs(angle[j]) < fabs(peak)) {
						npeak = j;
						peak = angle[j];
						pf = f + ((f / 2.0) * angle[j]);
					}
//					cerr << pf << endl;
					phase[j] = at2;

				//	cerr << f << ' ' << pf << ' ' << f + ((f / 2.0) * angle[j]) << ' ' << fci << ' ' << fcq << ' ' << ' ' << level[j] << ' ' << phase[j] << ' ' << peak << endl;

					j++;
				}
	
				double thisout = pf;	
//				cerr << pf << endl;

				if (f_post) thisout = f_post->feed(pf);	
				if (i > min_offset) {
					int bin = (thisout - 7600000) / 200000;
					if (1 || bin < 0) bin = 0;

					avglevel[bin] *= 0.9;
					avglevel[bin] += level[npeak] * .1;

//					if (fabs(shift) > 50000) thisout += shift;
//					cerr << ' ' << thisout << endl ;
//					out.push_back(((level[npeak] / avglevel[bin]) * 1200000) + 7600000); 
//					out.push_back((-(level[npeak] - 30) * (1500000.0 / 10.0)) + 9300000); 
					out.push_back(((level[npeak] / avglevel[bin]) > 0.3) ? thisout : 0);
//					out.push_back(thisout);
				};
				i++;
			}

//			cerr << total / in.size() << endl;
			return out;
		}
};

bool triple_hdyne = true;

int main(int argc, char *argv[])
{
	int rv = 0, fd = 0;
	long long dlen = -1;
	//double output[2048];
	unsigned char inbuf[2048];

	cerr << std::setprecision(10);
	cerr << argc << endl;
	cerr << strncmp(argv[1], "-", 1) << endl;

	if (argc >= 2 && (strncmp(argv[1], "-", 1))) {
		fd = open(argv[1], O_RDONLY);
	}

	if (argc >= 3) {
		unsigned long long offset = atoll(argv[2]);

		if (offset) lseek64(fd, offset, SEEK_SET);
	}
		
	if (argc >= 4) {
		if ((size_t)atoll(argv[3]) < dlen) {
			dlen = atoll(argv[3]); 
		}
	}

	cout << std::setprecision(8);
	
	rv = read(fd, inbuf, 2048);

	int i = 2048;
	
	Filter f_boost16(16, NULL, f_boost16_b);
	Filter f_boost32(32, NULL, f_boost32_b);
	Filter f_boost40(40, NULL, f_boost40_b);
	Filter f_boost48(48, NULL, f_boost48_b);
	Filter f_afilt16(16, NULL, f_afilt16_b);
	Filter f_afilt32(32, NULL, f_afilt32_b);

	double c[16];
	double total = 0;
	for (int i = 0; i < 16; i++) total += (1.0/(16.0));
	for (int i = 0; i < 16; i++) c[i] = (1.0/(16.0)) / total;
	
	Filter c_avg(15, NULL, c);

	//FM_demod video(2048, {7600000, 8100000, 8400000, 8700000, 9000000, 9300000}, NULL /*&f_boost16*/, {&f_lpf40_16, &f_lpf40_16, &f_lpf40_16, &f_lpf40_16, &f_lpf40_16, &f_lpf40_16}, NULL);

//	FM_demod video(2048, {8700000}, &f_boost16, {&f_lpf40_16, &f_lpf40_16, &f_lpf40_16, &f_lpf40_16, &f_lpf40_16, &f_lpf40_16}, NULL);
	FM_demod video(2048, {8100000, 8700000, 9300000}, {&f_boost40}, {&f_lpf, &f_lpf, &f_lpf}, NULL);
/*
	if (triple_hdyne) {
		FM_demod video(2048, {8100000, 8700000, 9300000}, {&f_boost32}, {&f_lpf, &f_lpf, &f_lpf}, NULL);
	} else {
		FM_demod video(2048, {8700000}, {&f_boost32}, {&f_lpf, &f_lpf, &f_lpf}, NULL);
	}
*/
//	FM_demod video(2048, {7600000, 8100000, 8400000, 8700000, 9000000, 9300000}, &f_boost16, {&f_lpf40_16, &f_lpf40_16, &f_lpf40_16, &f_lpf40_16, &f_lpf40_16, &f_lpf40_16}, NULL);
//	FM_demod video(2048, {7600000, 8100000, 8700000, 9300000}, &f_boost16, {&f_lpf40_16, &f_lpf40_16, &f_lpf40_16, &f_lpf40_16, &f_lpf40_16, &f_lpf40_16}, NULL);

	double charge = 0, prev = 8700000;

	while ((rv == 2048) && ((dlen == -1) || (i < dlen))) {
		vector<double> dinbuf;
		vector<unsigned short> ioutbuf;

		for (int j = 0; j < 2048; j++) dinbuf.push_back(inbuf[j]); 

		vector<double> outline = video.process(dinbuf);

		vector<unsigned short> bout;

		for (int i = 0; i < outline.size(); i++) {
			double n = outline[i];
			int in;

			if (n > 0) {
				double adj = pow(c_avg.feed(fabs(n - prev)) / 400000.0, 0.60);

//				cerr << i << ' ' << n << ' ';
				charge += ((n - prev) * 1.0);
				prev = n;

//				n -= (charge * (.68 - (adj * .105)));
//				n -= (charge * (.7 - (adj * .2)));
				//double f = (.75 - (adj * .50));
				double f = (.85 - (adj * .50));
				if (f < 0) f = 0;
				n -= (charge * f);
//				n -= (charge * .5);
		//		cerr << charge << ' ' << adj << ' ';
				charge *= 0.88;

//				cerr << n << ' ' << endl;

				n -= 7600000.0;
				n /= (9300000.0 - 7600000.0);
				if (n < 0) n = 0;
				in = 1 + (n * 57344.0);
				if (in > 65535) in = 65535;
//				cerr << in << endl;
			} else {
				in = 0;
			}

			bout.push_back(in);
		}
		
		unsigned short *boutput = bout.data();
		int len = outline.size();
		if (write(1, boutput, bout.size() * 2) != bout.size() * 2) {
			//cerr << "write error\n";
			exit(0);
		}

		i += (len > 1820) ? 1820 : len;
		memmove(inbuf, &inbuf[len], 2048 - len);
		rv = read(fd, &inbuf[(2048 - len)], len) + (2048 - len);
		
		if (rv < 2048) return 0;
		//cerr << i << ' ' << rv << endl;
	}
	return 0;
}

