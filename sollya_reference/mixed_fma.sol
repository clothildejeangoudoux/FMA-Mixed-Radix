
suppressmessage(433, 162, 183, 159, 51);

procedure round_integer(X, rnd) {
	  var res;

	  match (rnd) with
                RN : { res = nearestint(X); }
                RD : { res = floor(X); }
		RU : { res = ceil(X); }
		RZ : { if (X >= 0) then {
		       	  res = floor(X);
		       } else {
		       	  res = ceil(X);
		       };
		     };
          return res;
};

procedure __round_binary64_inner(X, rnd) {
	  var res;
	  var E, m;

	  E = max(floor(log2(X)) - 53 + 1, -1024 - 53 + 1 + 1 + 1);
	  m = round_integer(X * 2^-E, rnd);
	  if (m == 2^53) then {
	     E = E + 1;
	     m = m / 2;
	  };
	  if (E >= 1024 - 53 + 1) then {
	     match (rnd) with
	     	   RN : { res = infty; }
		   RU : { res = infty; }
		   RD : { res = 2^(1024 - 1 - 53 + 1) * (2^53 - 1); }
		   RZ : { res = 2^(1024 - 1 - 53 + 1) * (2^53 - 1); };
	  } else {
	     res = 2^E * m;
	  };

	  return res;
};

procedure round_binary64(X, rnd) {
	  var res;

	  if (X == 0) then {
	     res = X;
	  } else {
             if (X < 0) then {
                res = (match (rnd) with
                             RN : (-round_binary64(-X, RN))
                             RD : (-round_binary64(-X, RU))
                             RU : (-round_binary64(-X, RD))
                             RZ : (-round_binary64(-X, RZ)));
             } else {
                if (rnd == RZ) then {
                   res = __round_binary64_inner(X, RD);
                } else {
                   res = __round_binary64_inner(X, rnd);
                };
             };
          };

          return res;
};

procedure round_binary64_as_string(X, rnd) {
          var res, Y, s, E, m;
	  var oldprec, oldrationalmode;

          Y = round_binary64(X, rnd);

	  oldrationalmode = rationalmode;
	  rationalmode = on!;
	  oldprec = prec;
	  prec = 2000!;
	  Y := Y;
	  prec = oldprec!;
	  rationalmode = oldrationalmode!;

          if (Y == 0) then {
             res = "+ 0 0";
          } else {
             if (Y == infty) then {
                res = "0x7ff0000000000000";
             } else {
                if (Y == -infty) then {
                   res = "0xfff0000000000000";
                } else {
                   if (!(Y == Y)) then {
                      res = "0x7ff000000000cafe";
                   } else {
                      if (Y < 0) then {
                         s = "-";
                         E = exponent(-Y);
                         m = mantissa(-Y);
                      } else {
                         s = "+";
                         E = exponent(Y);
                         m = mantissa(Y);
                      };
                      res = s @ " " @ E @ " " @ m;
                   };
                };
             };
          };

          return res;
};

procedure __round_decimal64_innerst(X, rnd) {
          var res;
          var E, m, e;
	  var oldprec;

          E = max(floor(log10(X)) - 16 + 1, -385 - 16 + 1 + 1 + 1);
	
	  oldprec = prec;
	  prec = 1000!;
	  e := nearestint(mid(evaluate(E, [-1;1])));
	  E = e;
	
	  prec = oldprec!;

          m = round_integer(X * 10^-E, rnd);
	
          if (m == 10^16) then {
             E = E + 1;
             m = m / 10;
          };
          if (E >= 385 - 53 + 1) then {
             match (rnd) with
                   RN : { res = infty; }
                   RU : { res = infty; }
                   RD : { res = 10^(385 - 1 - 16 + 1) * (10^16 - 1); }
                   RZ : { res = 10^(385 - 1 - 16 + 1) * (10^16 - 1); };
          } else {
             res = 10^E * m;
          };

          return res;
};

procedure __round_decimal64_inner(X, rnd) {
	  var res;

	  if (X == nearestint(X)) then {
	     if ((1 <= X) && (X <= 10^16 - 1)) then {
	     	res = X;
	     } else {
	        res = __round_decimal64_innerst(X, rnd);
	     };
	  } else {
             res = __round_decimal64_innerst(X, rnd);
	  };

	  return res;
};

procedure round_decimal64(X, rnd) {
          var res;
	  var oldrationalmode;
	  
	  oldrationalmode = rationalmode;
	  rationalmode = on!;
          if (X == 0) then {
             res = X;
          } else {
             if (X < 0) then {
                res = (match (rnd) with
                             RN : (-round_decimal64(-X, RN))
                             RD : (-round_decimal64(-X, RU))
                             RU : (-round_decimal64(-X, RD))
                             RZ : (-round_decimal64(-X, RZ)));
             } else {
                if (rnd == RZ) then {
                   res = __round_decimal64_inner(X, RD);
                } else {
                   res = __round_decimal64_inner(X, rnd);
                };
             };
          };
	  rationalmode = oldrationalmode!;
		  
          return res;
};

procedure __exponent_mantissa_helper(X) {
          var res, t;
	  var oldrationalmode;
	  
	  oldrationalmode = rationalmode;
	  rationalmode = on!;
          if (X == 0) then {
             res = 0;
          } else {
             if (X == infty) then {
                res = 0;
             } else {
                if (X == -infty) then {
                   res = 0;
                } else {
                   if (!(X == X)) then {
                      res = 0;
                   } else {
                      if (floor(X) != X) then {
                         res = 0;
                      } else {
                         res = 0;
                         t = X;
                         while (floor(t/10) == t/10) do {
                               t = t / 10;
                               res = res + 1;
                         };
                      };
                   };
                };
             };
          };
	  rationalmode = oldrationalmode!;
          return res;
};

procedure __exponent_mantissa_decimal(X) {
          var E, m, t, e, p, q, s;
          var oldrationalmode;

          if (X == 0) then {
             E = 0;
             m = 0;
          } else {
             oldrationalmode = rationalmode;
             rationalmode = on!;
	     e := floor(log10(abs(X)));
             t = simplify(10^-e * X);
             p = numerator(t);
             q = denominator(t);
	     s := max(ceil(log10(abs(p))),ceil(log10(abs(q)))) + 200;
	     p = 10^s * p;
	     q = 10^s * q;
	     E = e + __exponent_mantissa_helper(p) - __exponent_mantissa_helper(q) - 1;
	     m := nearestint(10^-E * X * 10);
	     m = m / 10;
	     E = E + __exponent_mantissa_helper(m);
	     m := nearestint(10^-E * X * 10);
	     m = m / 10;

             while (floor(m) != m) do {
                m = m * 10;
		E = E - 1;
             };
	     rationalmode = oldrationalmode!;	
          };

          return { .E = E, .m = m };
};

procedure exponentDecimal(X) {
          var res;

          res = __exponent_mantissa_decimal(X);
          
          return res.E;
};

procedure mantissaDecimal(X) {
          var res;

          res = __exponent_mantissa_decimal(X);
          
          return res.m;
};

procedure round_decimal64_as_string(X, rnd) {
          var res, Y, s, E, m;
	  var oldprec, oldrationalmode;
	  
          Y = round_decimal64(X, rnd);
	  
	  oldrationalmode = rationalmode;
	  rationalmode = on!;
	  oldprec = prec;
	  prec = 2000!;
	  Y := Y;
	  prec = oldprec!;
	  rationalmode = oldrationalmode!;

          if (Y == 0) then {
             res = "+ 0 0";
          } else {
             if (Y == infty) then {
                res = "0x7c00000000000000";
             } else {
                if (Y == -infty) then {
                   res = "0xf800000000000000";
                } else {
                   if (!(Y == Y)) then {
                      res = "0x7c00000000000000";
                   } else {
                      if (Y < 0) then {
                         s = "-";
                         E = exponentDecimal(-Y);
                         m = mantissaDecimal(-Y);
                      } else {
                         s = "+";
                         E = exponentDecimal(Y);
                         m = mantissaDecimal(Y);
		      };
                      res = s @ " " @ E @ " " @ m;
                   };
                };
             };
          };

          return res;
};

procedure round_radix(X, rnd, XR) {
	  var res;

	  match (XR) with
	  	"B"     : { res = round_binary64(X, rnd); }
		"D"     : { res = round_decimal64(X, rnd); }
                default : { res = NaN;};

	  return res;
};

procedure to_string_radix(X, XR) {
	  var res;

	  match (XR) with
	  	"B"     : { res = round_binary64_as_string(X, RN); }
		"D"     : { res = round_decimal64_as_string(X, RN); }
                default : { res = "unsupported radix"; };

          return res;
};

procedure mixed_fma_as_string(X,Y,Z,rnd,XR,YR,ZR,WR) {
	  var XX, YY, ZZ, WW, W;
	  var oldrationalmode;
	  var res;
	  var inexact;

	  XX = round_radix(X, RN, XR);
	  YY = round_radix(Y, RN, YR);
	  ZZ = round_radix(Z, RN, ZR);

	  oldrationalmode = rationalmode;
	  rationalmode = on!;
	  WW = simplify(XX * YY + ZZ);
	  rationalmode = oldrationalmode!;

	  W = round_radix(WW, rnd, WR);

	  inexact = (W != WW);

	  res = to_string_radix(W, WR) @ " " @ to_string_radix(X, XR) @ " " @ to_string_radix(Y, YR) @ " " @ to_string_radix(Z, ZR);
	  if (inexact) then {
	     res = res @ " " @ "iouzp" @ " " @ "iouzP";
	  } else {
	     res = res @ " " @ "iouzp" @ " " @ "iouzp";
	  };
	  res = res @ " " @ rnd @ " " @ rnd;

	  return res;
};

