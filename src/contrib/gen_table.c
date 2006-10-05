/* $Id$ */
#include "math.h"
#include "string.h"
#include "copyrite.h"
#include "stdio.h"
#include "typedefs.h"
#include "macros.h"
#include "vec.h"
#include "statutil.h"
#include "coulomb.h"

enum { mGuillot, mAB1, mLjc, mMaaren, mGuillot_Maple, mHard_Wall, mGG_qd_q, mGG_qd_qd, mGG_q_q, mNR };

static double erf2(double x)
{
  return -(4*x/(sqrt(M_PI)))*exp(-x*x);
}

static double erf1(double x)
{
  return (2/sqrt(M_PI))*exp(-x*x);
}

void do_hard(FILE *fp,int pts_nm,double efac,double delta)
{
  int    i,k,imax;
  double x,vr,vr2,vc,vc2;
  
  if (delta < 0)
    gmx_fatal(FARGS,"Delta should be >= 0 rather than %f\n",delta);
    
  imax     = 3.0*pts_nm;
  for(i=0; (i<=imax); i++) {
    x   =  i*(1.0/pts_nm);
    
    if (x < delta) {
      /* Avoid very high numbers */
      vc = vc2 = 1/delta;
    }
    else {
      vc  = 1/(x);
      vc2 = 2/pow(x,3);
    }
    vr  = erfc(efac*(x-delta))/2;
    vr2 = (1-erf2(efac*(x-delta)))/2;
    fprintf(fp,"%12.5e  %12.5e  %12.5e  %12.5e  %12.5e  %12.5e  %12.5e\n",
	    x,vr,vr2,0.0,0.0,vc,vc2);
  }

}

void do_AB1(FILE *fp,int eel,int pts_nm,int ndisp,int nrep)
{
  int    i,k,imax;
  double myfac[3] = { 1, -1, 1 };
  double myexp[3] = { 1, 6, 0 };
  double x,v,v2;
  
  myexp[1] = ndisp;
  myexp[2] = nrep;
  imax     = 3.0*pts_nm;
  for(i=0; (i<=imax); i++) {
    x   =  i*(1.0/pts_nm);
    
    fprintf(fp,"%12.5e",x);
    
    for(k=0; (k<3); k++) {
      if (x < 0.04) {
	/* Avoid very high numbers */
	v = v2 = 0;
      }
      else {
	v  =  myfac[k]*pow(x,-myexp[k]);
	v2 = (myexp[k]+1)*(myexp[k])*v/(x*x); 
      }
      fprintf(fp,"  %12.5e  %12.5e",v,v2);
    }
    fprintf(fp,"\n");
  }
}

void lo_do_ljc(double r,
	       double *vc,double *fc,
	       double *vd,double *fd,
	       double *vr,double *fr)
{
  double r2,r_6,r_12;
  
  r2    = r*r;
  r_6   = 1.0/(r2*r2*r2);
  r_12  = r_6*r_6;

  *vc   = 1.0/r;
  *fc   = 1.0/(r2);

  *vd   = -r_6;
  *fd   = -6.0*(*vd)/r;

  *vr   = r_12;
  *fr   = 12.0*(*vr)/r;
}

/* use with coulombtype = user */
void lo_do_ljc_pme(double r,
		   double rcoulomb, double ewald_rtol,
		   double *vc,double *fc,
		   double *vd,double *fd,
		   double *vr,double *fr)
{
  double r2,r_6,r_12;
  double isp= 0.564189583547756;
  double ewc;

  ewc  = calc_ewaldcoeff(rcoulomb,ewald_rtol);

  r2   = r*r;
  r_6  = 1.0/(r2*r2*r2);
  r_12 = r_6*r_6;
  
  *vc   = erfc(ewc*r)/r;
  /* *vc2  = 2*erfc(ewc*r)/(r*r2)+4*exp(-(ewc*ewc*r2))*ewc*isp/r2+
     4*ewc*ewc*ewc*exp(-(ewc*ewc*r2))*isp;*/
  *fc  = 2*ewc*exp(-ewc*ewc*r2)*isp/r + erfc(ewc*r)/r2;

  *vd  = -r_6;
  *fd  = -6.0*(*vd)/r;

  *vr  = r_12;
  *fr  = 12.0*(*vr)/r;
}

void lo_do_guillot(double r,double xi, double xir,
			    double *vc,double *vc2,
			    double *vd,double *vd2,
			    double *vr,double *vr2)
{
  double qO     = -0.888;
  double qOd    = 0.226;
  double f0     = qOd/qO;
  double sqpi   = sqrt(M_PI);
  double r1,r2,z;
  
  r1    = r/(2*xi);
  r2    = r/(sqrt(2)*xi);
  *vc   = (1+sqr(f0)*erf(r1) + 2*f0*erf(r2))/r;
  *vc2  = ((2/sqr(r))*(*vc -
		       sqr(f0)*erf1(r1)/(2*xi) -
		       4*f0*erf1(r2)/sqrt(2)*xi) + 
	   (1/r)*(sqr(f0/(2.0*xi))*erf2(r1) + (2*f0/sqr(xi)))*erf2(r2));
  *vd   = -1.0/(r*r*r*r*r*r);
  *vd2  = 42.0*(*vd)/(r*r);
  z     = r/(2.0*xir);
  *vr   = erfc(z)/z;
  *vr2  = (sqpi*(*vr)/(2.0*z*z)+(1.0/(z*z)+1)*exp(-z*z))/(sqpi*sqr(xir));
}

void lo_do_guillot_maple(double r,double xi,double xir,
			 double *vc,double *vc2,
			 double *vd,double *vd2,
			 double *vr,double *vr2)
{
  double qO     = -0.888;
  double qOd    = 0.226;
  double f0     = qOd/qO;
  double sqpi   = sqrt(M_PI);

  *vc = pow(-f0/(1.0+f0)+1.0,2.0)/r+pow(-f0/(1.0+f0)+1.0,2.0)*f0*f0*erf(r/xi/2.0)/r+2.0*pow(-f0/(1.0+f0)+1.0,2.0)*f0*erf(r*sqrt(2.0)/xi/2.0)/r;
  *vc2 = 2.0*pow(-f0/(1.0+f0)+1.0,2.0)/(r*r*r)-pow(-f0/(1.0+f0)+1.0,2.0)*f0*f0/sqrt(M_PI)/(xi*xi*xi)*exp(-r*r/(xi*xi)/4.0)/2.0-2.0*pow(-f0/(1.0+f0)+1.0,2.0)*f0*f0/sqrt(M_PI)*exp(-r*r/(xi*xi)/4.0)/xi/(r*r)+2.0*pow(-f0/(1.0+f0)+1.0,2.0)*f0*f0*erf(r/xi/2.0)/(r*r*r)-2.0*pow(-f0/(1.0+f0)+1.0,2.0)*f0/sqrt(M_PI)/(xi*xi*xi)*exp(-r*r/(xi*xi)/2.0)*sqrt(2.0)-4.0*pow(-f0/(1.0+f0)+1.0,2.0)*f0/sqrt(M_PI)*exp(-r*r/(xi*xi)/2.0)*sqrt(2.0)/xi/(r*r)+4.0*pow(-f0/(1.0+f0)+1.0,2.0)*f0*erf(r*sqrt(2.0)/xi/2.0)/(r*r*r);
  
  *vd   = -1.0/(r*r*r*r*r*r);
  *vd2  = -42.0/(r*r*r*r*r*r*r*r);
  *vr   = 2.0*erfc(r/xir/2.0)/r*xir;
  *vr2  = 1.0/sqrt(M_PI)/(xir*xir)*exp(-r*r/(xir*xir)/4.0)+4.0/sqrt(M_PI)*exp(-r*r/(xir*xir)/4.0)/(r*r)+4.0*erfc(r/xir/2.0)/(r*r*r)*xir;
}

/* Guillot2001 diffuse charge - diffuse charge interaction
   MuPAD
   Uqdqd := erf(r/(2*xi))/r;
   dUqdqd := diff(Uqdqd,r);
   d2Uqdqd := diff(dUqdqd,r);
   simplify(%);
   generate::C(d2Uqdqd); 
*/
void lo_do_GG_qd_qd(double r,double xi,double xir,
		    double *vc,double *vc2,
		    double *vd,double *vd2,
		    double *vr,double *vr2)
{
  double sqpi   = sqrt(M_PI);

  *vc = erf((r*(1.0/2.0))/xi)/r;
  *vc2 = 2.0*pow(r, -3.0)*erf((r*(1.0/2.0))/xi) - (1.0/2.0)*pow(M_PI, -1.0/2.0)*pow(xi, -3.0)*exp((-1.0/4.0)*(r*r)*pow(xi, -2.0)) - (2.0*pow(M_PI, -1.0/2.0)*pow(r, -2.0)*exp((-1.0/4.0)*(r*r)*pow(xi, -2.0)))/xi ;
  *vd   = 0.0;
  *vd2  = 0.0;
  *vr   = 0.0;
  *vr2  = 0.0;
}

/* Guillot2001 diffuse charge - charge interaction
   MuPAD
   Uqdqd := erf(r/(sqrt(2)*xi))/r;
   dUqdqd := diff(Uqdqd,r);
   d2Uqdqd := diff(dUqdqd,r);
   simplify(%);
   generate::C(Uqdqd);
   generate::C(d2Uqdqd);
*/
void lo_do_GG_qd_q(double r,double xi,double xir,
		   double *vc,double *vc2,
		   double *vd,double *vd2,
		   double *vr,double *vr2)
{
  double sqpi   = sqrt(M_PI);

  *vc = erf(((1.0/2.0)*pow(2.0, 1.0/2.0)*r)/xi)/r ;
  *vc2 = 2.0*pow(r, -3.0)*erf(((1.0/2.0)*pow(2.0, 1.0/2.0)*r)/xi) - pow(2.0, 1.0/2.0)*pow(M_PI, -1.0/2.0)*pow(xi, -3.0)*exp((-1.0/2.0)*(r*r)*pow(xi, -2.0)) - (2.0*pow(2.0, 1.0/2.0)*pow(M_PI, -1.0/2.0)*pow(r, -2.0)*exp((-1.0/2.0)*(r*r)*pow(xi, -2.0)))/xi ;

  *vd   = 0.0;
  *vd2  = 0.0;
  *vr   = 0.0;
  *vr2  = 0.0;
}

/* Guillot2001 charge - charge interaction (normal coulomb), 
   repulsion and dispersion
   MuPAD  
   Uqq := 1/r;
   dUqq := diff(Uqq,r);
   d2Uqq := diff(dUqq,r);
   
   Ud := -1/r^6;
   dUd := diff(Ud,r);
   d2Ud := diff(dUd,r);
   
   z     := r/(2*xir);
   Ur    := erfc(z)/z;
   dUr   := diff(Ur,r);
   d2Ur  := diff(dUr,r);
   simplify(%);
   generate::C(Ur);
   generate::C(d2Ur);
*/
void lo_do_GG_q_q(double r,double xi,double xir,
		  double *vc,double *vc2,
		  double *vd,double *vd2,
		  double *vr,double *vr2)
{
  double sqpi   = sqrt(M_PI);

  *vc   = 1.0/r;
  *vc2  = 2.0/(r*r*r);
  *vd   = -1.0/(r*r*r*r*r*r);
  *vd2  = -42.0/(r*r*r*r*r*r*r*r);
  *vr   = (2.0*xir*erfc((r*(1.0/2.0))/xir))/r;
  *vr2  = 4.0*pow(M_PI, -1.0/2.0)*pow(r, -2.0)*exp((-1.0/4.0)*(r*r)*pow(xir, -2.0)) + pow(M_PI, -1.0/2.0)*pow(xir, -2.0)*exp((-1.0/4.0)*(r*r)*pow(xir, -2.0)) + 4.0*pow(r, -3.0)*xir*erfc((r*(1.0/2.0))/xir);
}

static void do_guillot(FILE *fp,int eel,int pts_nm,double rc,double rtol,double xi,double xir)
{
  int    i,i0,imax;
  double r,vc,vc2,vd,vd2,vr,vr2;

  imax = 3*pts_nm;
  for(i=0; (i<=imax); i++) {
    r     = i*(1.0/pts_nm);
    /* Avoid very large numbers */
    if (r < 0.04) {
      vc = vc2 = vd = vd2 = vr = vr2 = 0;
    }
    else 
      lo_do_guillot(r,xi,xir,&vc,&vc2,&vd,&vd2,&vr,&vr2);
    fprintf(fp,"%12.5e  %12.5e  %12.5e   %12.5e  %12.5e  %12.5e  %12.5e\n",
	    r,vc,vc2,vd,vd2,vr,vr2);
  }
}

static void do_ljc(FILE *fp,int eel,int pts_nm,real rc,real rtol)
{
  int    i,i0,imax;
  double r,vc,fc,vd,fd,vr,fr;

  imax = 3*pts_nm;
  for(i=0; (i<=imax); i++) {
    r     = i*(1.0/pts_nm);
    /* Avoid very large numbers */
    if (r < 0.04) {
      vc = fc = vd = fd = vr = fr = 0;
    } else {
      if (eel == eelPME) {
	lo_do_ljc_pme(r,rc,rtol,&vc,&fc,&vd,&fd,&vr,&fr);
      } else if (eel == eelCUT) { 
	lo_do_ljc(r,&vc,&fc,&vd,&fd,&vr,&fr);
      }
    }
    fprintf(fp,"%15.10e   %15.10e %15.10e   %15.10e %15.10e   %15.10e %15.10e\n",
	    r,vc,fc,vd,fd,vr,fr);
  }
}

static void do_guillot_maple(FILE *fp,int eel,int pts_nm,double rc,double rtol,double xi,double xir)
{
  int    i,i0,imax;
  /*  double xi     = 0.15;*/
  double r,vc,vc2,vd,vd2,vr,vr2;

  imax = 3*pts_nm;
  for(i=0; (i<=imax); i++) {
    r     = i*(1.0/pts_nm);
    /* Avoid very large numbers */
    if (r < 0.04) {
      vc = vc2 = vd = vd2 = vr = vr2 = 0;
    }
    else
      if (eel == eelPME) {
	fprintf(fp, "Not implemented\n");
      } else if (eel == eelCUT) { 
	lo_do_guillot_maple(r,xi,xir,&vc,&vc2,&vd,&vd2,&vr,&vr2);
      }
    fprintf(fp,"%12.5e  %12.5e  %12.5e   %12.5e  %12.5e  %12.5e  %12.5e\n",
	    r,vc,vc2,vd,vd2,vr,vr2);
  }
} 

static void do_GG_q_q(FILE *fp,int eel,int pts_nm,double rc,double rtol,double xi,double xir)
{
  int    i,i0,imax;
  double r,vc,vc2,vd,vd2,vr,vr2;

  imax = 3*pts_nm;
  for(i=0; (i<=imax); i++) {
    r     = i*(1.0/pts_nm);
    /* Avoid very large numbers */
    if (r < 0.04) {
      vc = vc2 = vd = vd2 = vr = vr2 = 0;
    }
    else
      if (eel == eelPME) {
	fprintf(fp, "Not implemented\n");
      } else if (eel == eelCUT) { 
	lo_do_GG_q_q(r,xi,xir,&vc,&vc2,&vd,&vd2,&vr,&vr2);
      }
    fprintf(fp,"%12.5e  %12.5e  %12.5e   %12.5e  %12.5e  %12.5e  %12.5e\n",
	    r,vc,vc2,vd,vd2,vr,vr2);
  }
} 

static void do_GG_qd_q(FILE *fp,int eel,int pts_nm,double rc,double rtol,double xi,double xir)
{
  int    i,i0,imax;
  /*  double xi     = 0.15;*/
  double r,vc,vc2,vd,vd2,vr,vr2;

  imax = 3*pts_nm;
  for(i=0; (i<=imax); i++) {
    r     = i*(1.0/pts_nm);
    /* Avoid very large numbers */
    if (r < 0.04) {
      vc = vc2 = vd = vd2 = vr = vr2 = 0;
    }
    else
      if (eel == eelPME) {
	fprintf(fp, "Not implemented\n");
      } else if (eel == eelCUT) { 
	lo_do_GG_qd_q(r,xi,xir,&vc,&vc2,&vd,&vd2,&vr,&vr2);
      }
    fprintf(fp,"%12.5e  %12.5e  %12.5e   %12.5e  %12.5e  %12.5e  %12.5e\n",
	    r,vc,vc2,vd,vd2,vr,vr2);
  }
} 

static void do_GG_qd_qd(FILE *fp,int eel,int pts_nm,double rc,double rtol,double xi,double xir)
{
  int    i,i0,imax;
  /*  double xi     = 0.15;*/
  double r,vc,vc2,vd,vd2,vr,vr2;

  imax = 3*pts_nm;
  for(i=0; (i<=imax); i++) {
    r     = i*(1.0/pts_nm);
    /* Avoid very large numbers */
    if (r < 0.04) {
      vc = vc2 = vd = vd2 = vr = vr2 = 0;
    }
    else
      if (eel == eelPME) {
	fprintf(fp, "Not implemented\n");
      } else if (eel == eelCUT) { 
	lo_do_GG_qd_qd(r,xi,xir,&vc,&vc2,&vd,&vd2,&vr,&vr2);
      }
    fprintf(fp,"%12.5e  %12.5e  %12.5e   %12.5e  %12.5e  %12.5e  %12.5e\n",
	    r,vc,vc2,vd,vd2,vr,vr2);
  }
} 

static void do_maaren(FILE *fp,int eel,int pts_nm,int npow)
{
  int    i,i0,imax;
  double xi     = 0.05;
  double xir     = 0.0615;
  double r,vc,vc2,vd,vd2,vr,vr2;

  imax = 3*pts_nm;
  for(i=0; (i<=imax); i++) {
    r     = i*(1.0/pts_nm);
    /* Avoid very large numbers */
    if (r < 0.04) {
      vc = vc2 = vd = vd2 = vr = vr2 = 0;
    }
    else {
      lo_do_guillot_maple(r,xi,xir,&vc,&vc2,&vd,&vd2,&vr,&vr2);
      vr  =  pow(r,-1.0*npow);
      vr2 = (npow+1.0)*(npow)*vr/sqr(r); 
    }
    fprintf(fp,"%12.5e  %12.5e  %12.5e   %12.5e  %12.5e  %12.5e  %12.5e\n",
	    r,vc,vc2,vd,vd2,vr,vr2);
  }
}

int main(int argc,char *argv[])
{
  static char *desc[] = {
    "gen_table generates tables for mdrun for use with the USER defined",
    "potentials. Note that the format has been update for higher",
    "accuracy ni the forces starting with version 4.0. Using older",
    "tables with 4.0 will silently crash your simulations, as will",
    "using new tables with an older GROMACS version."
  };
  static char *opt[]     = { NULL, "cut", "rf", "pme", NULL };
  /*  static char *model[]   = { NULL, "guillot", "AB1", "ljc", "maaren", "guillot_maple", "hard_wall", "gg_q_q", "gg_qd_q", "gg_qd_qd", NULL }; */
  static char *model[]   = { NULL, "ljc", NULL };
  static real delta=0,efac=500,rc=0.9,rtol=1e-05,xi=0.15,xir=0.0615;
  static int  nrep       = 12;
  static int  ndisp      = 6;
  static int  pts_nm     = 500;
  t_pargs pa[] = {
    { "-el",     FALSE, etENUM, {opt},
      "Electrostatics type: cut, rf or pme" },
    { "-rc",     FALSE, etREAL, {&rc},
      "Cut-off required for rf or pme" },
    { "-rtol",   FALSE, etREAL, {&rtol},
      "Ewald tolerance required for pme" },
    { "-xi",   FALSE, etREAL, {&xi},
      "Width of the Gaussian diffuse charge of the G&G model" },
    { "-xir",   FALSE, etREAL, {&xir},
      "Width of erfc(z)/z repulsion of the G&G model (z=0.5 rOO/xir)" },
    { "-m",      FALSE, etENUM, {model},
      "Model for the tables" },
    { "-resol",  FALSE, etINT,  {&pts_nm},
      "Resolution of the table (points per nm)" },
    { "-delta",  FALSE, etREAL, {&delta},
      "Displacement in the Coulomb functions (nm), used as 1/(r+delta). Only for hard wall potential." },
    { "-efac",   FALSE, etREAL, {&efac},
      "Number indicating the steepness of the hardwall potential." },
    { "-nrep",   FALSE, etINT,  {&nrep},
      "Power for the repulsion potential (with model AB1 or maaren)" },
    { "-ndisp",   FALSE, etINT,  {&ndisp},
      "Power for the dispersion potential (with model AB1 or maaren)" }
  };
#define NPA asize(pa)
  t_filenm fnm[] = {
    { efXVG, "-o", "table", ffWRITE }
  };
#define NFILE asize(fnm)
  FILE *fp;
  int  eel=0,m=0;
  
  CopyRight(stderr,argv[0]);
  parse_common_args(&argc,argv,PCA_CAN_VIEW | PCA_CAN_TIME | PCA_BE_NICE,
		    NFILE,fnm,NPA,pa,asize(desc),desc,0,NULL);
  
  if (strcmp(opt[0],"cut") == 0) 
    eel = eelCUT;
  else if (strcmp(opt[0],"rf") == 0) 
    eel = eelRF;
  else if (strcmp(opt[0],"pme") == 0) 
    eel = eelPME;
  else 
    gmx_fatal(FARGS,"Invalid argument %s for option -e",opt[0]);
  if (strcmp(model[0],"maaren") == 0) 
    m = mMaaren;
  else if (strcmp(model[0],"AB1") == 0) 
    m = mAB1;
  else if (strcmp(model[0],"ljc") == 0) 
    m = mLjc;
  else if (strcmp(model[0],"guillot") == 0) 
    m = mGuillot;
  else if (strcmp(model[0],"guillot_maple") == 0) 
    m = mGuillot_Maple;
  else if (strcmp(model[0],"hard_wall") == 0) 
    m = mHard_Wall;
  else if (strcmp(model[0],"gg_qd_q") == 0) 
    m = mGG_qd_q;
  else if (strcmp(model[0],"gg_qd_qd") == 0) 
    m = mGG_qd_qd;
  else if (strcmp(model[0],"gg_q_q") == 0) 
    m = mGG_q_q;
  else 
    gmx_fatal(FARGS,"Invalid argument %s for option -m",opt[0]);
    
  fp = ffopen(opt2fn("-o",NFILE,fnm),"w");
  switch (m) {
  case mGuillot:
    do_guillot(fp,eel,pts_nm,rc,rtol,xi,xir);
    break;
  case mGuillot_Maple:
    fprintf(fp, "#\n# Table Guillot_Maple: rc=%g, rtol=%g, xi=%g, xir=%g\n#\n",rc,rtol,xi,xir);
    do_guillot_maple(fp,eel,pts_nm,rc,rtol,xi,xir);
    break;
  case mGG_q_q:
    fprintf(fp, "#\n# Table GG_q_q: rc=%g, rtol=%g, xi=%g, xir=%g\n#\n",rc,rtol,xi,xir);
    do_GG_q_q(fp,eel,pts_nm,rc,rtol,xi,xir);
    break;
  case mGG_qd_q:
    fprintf(stdout, "case mGG_qd_q");
    fprintf(fp, "#\n# Table GG_qd_q: rc=%g, rtol=%g, xi=%g, xir=%g\n#\n",rc,rtol,xi,xir);
    do_GG_qd_q(fp,eel,pts_nm,rc,rtol,xi,xir);
    break;
  case mGG_qd_qd:
    fprintf(stdout, "case mGG_qd_qd");
    fprintf(fp, "#\n# Table GG_qd_qd: rc=%g, rtol=%g, xi=%g, xir=%g\n#\n",rc,rtol,xi,xir);
    do_GG_qd_qd(fp,eel,pts_nm,rc,rtol,xi,xir);
    break;
  case mMaaren:
    do_maaren(fp,eel,pts_nm,nrep);
    break;
  case mAB1:
    fprintf(fp, "#\n# Table AB1: ndisp=%d nrep=%d\n#\n",ndisp,nrep);
    do_AB1(fp,eel,pts_nm,ndisp,nrep);
    break;
  case mLjc:
    fprintf(fp, "#\n# Table LJC(12-6-1): rc=%g, rtol=%g\n#\n",rc,rtol);
    do_ljc(fp,eel,pts_nm,rc,rtol);
    break;
  case mHard_Wall:
    do_hard(fp,pts_nm,efac,delta);
    break;
  default:
    gmx_fatal(FARGS,"Model %s not supported yet",model[0]);
  }  
  fclose(fp);
  
  thanx(stdout);
  
  return 0;
}
