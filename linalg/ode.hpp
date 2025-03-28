// Copyright (c) 2010-2025, Lawrence Livermore National Security, LLC. Produced
// at the Lawrence Livermore National Laboratory. All Rights reserved. See files
// LICENSE and NOTICE for details. LLNL-CODE-806117.
//
// This file is part of the MFEM library. For more information and source code
// availability visit https://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the BSD-3 license. We welcome feedback and contributions, see file
// CONTRIBUTING.md for details.

#ifndef MFEM_ODE
#define MFEM_ODE

#include "../general/communication.hpp"
#include "../config/config.hpp"
#include "operator.hpp"
#include <vector>
#include <memory>

namespace mfem
{

/// An interface for storing state of previous timesteps
class ODEStateData
{
public:
   /// Get the maximum number of stored stages
   virtual int MaxSize() const = 0;

   /// Get the current number of stored stages
   virtual int Size() const = 0;

   /// Get the ith state vector
   virtual const Vector &Get(int i) const = 0;

   /// Get the ith state vector - non-const version
   virtual Vector &Get(int i) = 0;

   /// Get the ith state vector - with a copy
   virtual void Get(int i, Vector &vec) const = 0;

   /// Set the ith state vector
   virtual void Set(int i, Vector &state) = 0;

   /// Add state vector and increment state size
   virtual void Append(Vector &state) = 0;

   /// Virtual destructor
   virtual ~ODEStateData() = default;
};

/// An implementation of ODEStateData that stores states in an std::vector<Vector>
class ODEStateDataVector : public ODEStateData
{
private:
   MemoryType mem_type;
   int ss, smax;
   std::vector<Vector> data;
   Array<int> idx;

public:
   ODEStateDataVector (int smax): smax(smax)
   {
      data.resize(smax);
      idx.SetSize(smax);
      ss = 0;
   };

   /// Set the number of stages and the size of the vectors
   void  SetSize(int vsize, MemoryType mem_type);

   /// Shift the stage counter for the next timestep
   inline void ShiftStages()
   {
      for (int i = 0; i < smax; i++) { idx[i] = (++idx[i])%smax; }
   };

   /// Increment the stage counter
   void Increment() { ss++; ss = std::min(ss,smax); };

   /// Reset the stage counter
   void Reset() { ss = 0; };

   /// Reference access to the ith vector.
   inline Vector & operator[](int i) { return data[idx[i]]; };

   /// Const reference access to the ith vector.
   inline const Vector &operator[](int i) const { return data[idx[i]]; };

   /// Print state data
   void Print(std::ostream &os = mfem::out) const ;

   int  MaxSize() const override { return smax; };

   int  Size() const override { return ss; };

   const Vector &Get(int i) const override;
   Vector &Get(int i) override;
   void Get(int i, Vector &vec) const override;

   void Set(int i, Vector &state) override;

   void Append(Vector &state) override;
};


/// Abstract class for solving systems of ODEs: dx/dt = f(x,t)
class ODESolver
{
protected:
   /// Pointer to the associated TimeDependentOperator.
   TimeDependentOperator *f;  // f(.,t) : R^n --> R^n
   MemoryType mem_type;

public:
   ODESolver() : f(NULL) { mem_type = Device::GetHostMemoryType(); }

   /// Associate a TimeDependentOperator with the ODE solver.
   /** This method has to be called:
       - Before the first call to Step().
       - When the dimensions of the associated TimeDependentOperator change.
       - When a time stepping sequence has to be restarted.
       - To change the associated TimeDependentOperator. */
   virtual void Init(TimeDependentOperator &f_);

   /** @brief Perform a time step from time @a t [in] to time @a t [out] based
       on the requested step size @a dt [in]. */
   /** @param[in,out] x   Approximate solution.
       @param[in,out] t   Time associated with the approximate solution @a x.
       @param[in,out] dt  Time step size.

       The following rules describe the common behavior of the method:
       - The input @a x [in] is the approximate solution for the input time
         @a t [in].
       - The input @a dt [in] is the desired time step size, defining the desired
         target time: t [target] = @a t [in] + @a dt [in].
       - The output @a x [out] is the approximate solution for the output time
         @a t [out].
       - The output @a dt [out] is the last time step taken by the method which
         may be smaller or larger than the input @a dt [in] value, e.g. because
         of time step control.
       - The method may perform more than one time step internally; in this case
         @a dt [out] is the last internal time step size.
       - The output value of @a t [out] may be smaller or larger than
         t [target], however, it is not smaller than @a t [in] + @a dt [out], if
         at least one internal time step was performed.
       - The value @a x [out] may be obtained by interpolation using internally
         stored data.
       - In some cases, the contents of @a x [in] may not be used, e.g. when
         @a x [out] from a previous Step() call was obtained by interpolation.
       - In consecutive calls to this method, the output @a t [out] of one
         Step() call has to be the same as the input @a t [in] to the next
         Step() call.
       - If the previous rule has to be broken, e.g. to restart a time stepping
         sequence, then the ODE solver must be re-initialized by calling Init()
         between the two Step() calls. */
   virtual void Step(Vector &x, real_t &t, real_t &dt) = 0;

   /// Perform time integration from time @a t [in] to time @a tf [in].
   /** @param[in,out] x   Approximate solution.
       @param[in,out] t   Time associated with the approximate solution @a x.
       @param[in,out] dt  Time step size.
       @param[in]     tf  Requested final time.

       The default implementation makes consecutive calls to Step() until
       reaching @a tf.
       The following rules describe the common behavior of the method:
       - The input @a x [in] is the approximate solution for the input time
         @a t [in].
       - The input @a dt [in] is the initial time step size.
       - The output @a dt [out] is the last time step taken by the method which
         may be smaller or larger than the input @a dt [in] value, e.g. because
         of time step control.
       - The output value of @a t [out] is not smaller than @a tf [in]. */
   virtual void Run(Vector &x, real_t &t, real_t &dt, real_t tf)
   {
      while (t < tf) { Step(x, t, dt); }
   }

   /// Returns how many State vectors the ODE requires
   virtual int GetStateSize() { return 0; };

   // Help info for ODESolver options
   static MFEM_EXPORT std::string ExplicitTypes;
   static MFEM_EXPORT std::string ImplicitTypes;
   static MFEM_EXPORT std::string Types;

   /// Function for selecting the desired ODESolver (Explicit and Implicit)
   /// Returns an ODESolver pointer based on an type
   /// Caller gets ownership of the object and is responsible for its deletion
   static MFEM_EXPORT std::unique_ptr<ODESolver> Select(const int ode_solver_type);

   /// Function for selecting the desired Explicit ODESolver
   /// Returns an ODESolver pointer based on an type
   /// Caller gets ownership of the object and is responsible for its deletion
   static MFEM_EXPORT std::unique_ptr<ODESolver> SelectExplicit(
      const int ode_solver_type);

   /// Function for selecting the desired Implicit ODESolver
   /// Returns an ODESolver pointer based on an type
   /// Caller gets ownership of the object and is responsible for its deletion
   static MFEM_EXPORT std::unique_ptr<ODESolver> SelectImplicit(
      const int ode_solver_type);

   virtual ~ODESolver() { }
};

/// Abstract class for an ODESolver that has state history implemented as ODEStateData
class ODESolverWithStates : public ODESolver
{
public:
   /// Returns the StateData
   virtual ODEStateData& GetState() = 0;

   /// Returns the StateData
   virtual const ODEStateData& GetState() const = 0;

   /// Returns how many State vectors the ODE requires
   virtual int GetStateSize() { return GetState().MaxSize(); };
};


/// The classical forward Euler method
class ForwardEulerSolver : public ODESolver
{
private:
   Vector dxdt;

public:
   void Init(TimeDependentOperator &f_) override;

   void Step(Vector &x, real_t &t, real_t &dt) override;
};


/** A family of explicit second-order RK2 methods. Some choices for the
    parameter 'a' are:
    a = 1/2 - the midpoint method
    a =  1  - Heun's method
    a = 2/3 - default, has minimal truncation error. */
class RK2Solver : public ODESolver
{
private:
   real_t a;
   Vector dxdt, x1;

public:
   RK2Solver(const real_t a_ = 2./3.) : a(a_) { }

   void Init(TimeDependentOperator &f_) override;

   void Step(Vector &x, real_t &t, real_t &dt) override;
};


/// Third-order, strong stability preserving (SSP) Runge-Kutta method
class RK3SSPSolver : public ODESolver
{
private:
   Vector y, k;

public:
   void Init(TimeDependentOperator &f_) override;

   void Step(Vector &x, real_t &t, real_t &dt) override;
};


/// The classical explicit forth-order Runge-Kutta method, RK4
class RK4Solver : public ODESolver
{
private:
   Vector y, k, z;

public:
   void Init(TimeDependentOperator &f_) override;

   void Step(Vector &x, real_t &t, real_t &dt) override;
};


/** An explicit Runge-Kutta method corresponding to a general Butcher tableau
    +--------+----------------------+
    | c[0]   | a[0]                 |
    | c[1]   | a[1] a[2]            |
    | ...    |    ...               |
    | c[s-2] | ...   a[s(s-1)/2-1]  |
    +--------+----------------------+
    |        | b[0] b[1] ... b[s-1] |
    +--------+----------------------+ */
class ExplicitRKSolver : public ODESolver
{
private:
   int s;
   const real_t *a, *b, *c;
   Vector y, *k;

public:
   ExplicitRKSolver(int s_, const real_t *a_, const real_t *b_,
                    const real_t *c_);

   void Init(TimeDependentOperator &f_) override;

   void Step(Vector &x, real_t &t, real_t &dt) override;

   virtual ~ExplicitRKSolver();
};


/** An 8-stage, 6th order RK method. From Verner's "efficient" 9-stage 6(5)
    pair. */
class RK6Solver : public ExplicitRKSolver
{
private:
   static MFEM_EXPORT const real_t a[28], b[8], c[7];

public:
   RK6Solver() : ExplicitRKSolver(8, a, b, c) { }
};


/** A 12-stage, 8th order RK method. From Verner's "efficient" 13-stage 8(7)
    pair. */
class RK8Solver : public ExplicitRKSolver
{
private:
   static MFEM_EXPORT const real_t a[66], b[12], c[11];

public:
   RK8Solver() : ExplicitRKSolver(12, a, b, c) { }
};


/// Backward Euler ODE solver. L-stable.
class BackwardEulerSolver : public ODESolver
{
protected:
   Vector k;

public:
   void Init(TimeDependentOperator &f_) override;

   void Step(Vector &x, real_t &t, real_t &dt) override;
};


/// Implicit midpoint method. A-stable, not L-stable.
class ImplicitMidpointSolver : public ODESolver
{
protected:
   Vector k;

public:
   void Init(TimeDependentOperator &f_) override;

   void Step(Vector &x, real_t &t, real_t &dt) override;
};


/** Two stage, singly diagonal implicit Runge-Kutta (SDIRK) methods;
    the choices for gamma_opt are:
    0 - 3rd order method, not A-stable
    1 - 3rd order method, A-stable, not L-stable (default)
    2 - 2nd order method, L-stable
    3 - 2nd order method, L-stable (has solves outside [t,t+dt]). */
class SDIRK23Solver : public ODESolver
{
protected:
   real_t gamma;
   Vector k, y;

public:
   SDIRK23Solver(int gamma_opt = 1);

   void Init(TimeDependentOperator &f_) override;

   void Step(Vector &x, real_t &t, real_t &dt) override;
};


/** Three stage, singly diagonal implicit Runge-Kutta (SDIRK) method of
    order 4. A-stable, not L-stable. */
class SDIRK34Solver : public ODESolver
{
protected:
   Vector k, y, z;

public:
   void Init(TimeDependentOperator &f_) override;

   void Step(Vector &x, real_t &t, real_t &dt) override;
};


/** Three stage, singly diagonal implicit Runge-Kutta (SDIRK) method of
    order 3. L-stable. */
class SDIRK33Solver : public ODESolver
{
protected:
   Vector k, y;

public:
   void Init(TimeDependentOperator &f_) override;

   void Step(Vector &x, real_t &t, real_t &dt) override;
};


/** Two stage, explicit singly diagonal implicit Runge-Kutta (ESDIRK) method
    of order 2. A-stable. */
class TrapezoidalRuleSolver : public ODESolver
{
protected:
   Vector k, y;

public:
   void Init(TimeDependentOperator &f_) override;

   void Step(Vector &x, real_t &t, real_t &dt) override;
};


/** Three stage, explicit singly diagonal implicit Runge-Kutta (ESDIRK) method
    of order 2. L-stable. */
class ESDIRK32Solver : public ODESolver
{
protected:
   Vector k, y, z;

public:
   void Init(TimeDependentOperator &f_) override;

   void Step(Vector &x, real_t &t, real_t &dt) override;
};


/** Three stage, explicit singly diagonal implicit Runge-Kutta (ESDIRK) method
    of order 3. A-stable. */
class ESDIRK33Solver : public ODESolver
{
protected:
   Vector k, y, z;

public:
   void Init(TimeDependentOperator &f_) override;

   void Step(Vector &x, real_t &t, real_t &dt) override;
};


/// Generalized-alpha ODE solver from "A generalized-α method for integrating
/// the filtered Navier-Stokes equations with a stabilized finite element
/// method" by K.E. Jansen, C.H. Whiting and G.M. Hulbert.
class GeneralizedAlphaSolver : public ODESolverWithStates
{
   ODEStateDataVector state;

protected:

   mutable Vector k,y;
   real_t alpha_f, alpha_m, gamma;

   void SetRhoInf(real_t rho_inf);
   void PrintProperties(std::ostream &os = mfem::out);
public:

   GeneralizedAlphaSolver(real_t rho = 1.0) : state(1) { SetRhoInf(rho); };
   void Init(TimeDependentOperator &f_) override;
   void Step(Vector &x, real_t &t, real_t &dt) override;

   ODEStateData& GetState() override { return state; }
   const ODEStateData& GetState() const override { return state; }
};


/** An explicit Adams-Bashforth method. */
class AdamsBashforthSolver : public ODESolverWithStates
{
private:
   const real_t *a;
   const int stages;
   real_t dt_;
   ODEStateDataVector state;

protected:
   std::unique_ptr<ODESolver> RKsolver;

   inline bool print()
   {
#ifdef MFEM_USE_MPI
      return Mpi::IsInitialized() ? Mpi::Root() : true;
#else
      return true;
#endif
   }

   void CheckTimestep(real_t dt);

public:
   AdamsBashforthSolver(int s_, const real_t *a_);
   void Init(TimeDependentOperator &f_) override;
   void Step(Vector &x, real_t &t, real_t &dt) override;

   ODEStateData& GetState() override { return state; }
   const ODEStateData& GetState() const override { return state; }
};

/** A 1-stage, 1st order AB method.  */
class AB1Solver : public AdamsBashforthSolver
{
private:
   static MFEM_EXPORT const real_t a[1];

public:
   AB1Solver() : AdamsBashforthSolver(1, a) { }
};

/** A 2-stage, 2nd order AB method.  */
class AB2Solver : public AdamsBashforthSolver
{
private:
   static MFEM_EXPORT const real_t a[2];

public:
   AB2Solver() : AdamsBashforthSolver(2, a) { RKsolver.reset(new RK2Solver()); }
};

/** A 3-stage, 3rd order AB method.  */
class AB3Solver : public AdamsBashforthSolver
{
private:
   static MFEM_EXPORT const real_t a[3];

public:
   AB3Solver() : AdamsBashforthSolver(3, a) { RKsolver.reset(new RK3SSPSolver()); }
};

/** A 4-stage, 4th order AB method.  */
class AB4Solver : public AdamsBashforthSolver
{
private:
   static MFEM_EXPORT const real_t a[4];

public:
   AB4Solver() : AdamsBashforthSolver(4, a) { RKsolver.reset(new RK4Solver()); }
};

/** A 5-stage, 5th order AB method.  */
class AB5Solver : public AdamsBashforthSolver
{
private:
   static MFEM_EXPORT const real_t a[5];

public:
   AB5Solver() : AdamsBashforthSolver(5, a) { RKsolver.reset(new RK6Solver()); }
};


/** An implicit Adams-Moulton method. */
class AdamsMoultonSolver : public ODESolverWithStates
{
private:
   const real_t *a;
   const int stages;
   real_t dt_;
   ODEStateDataVector state;

protected:
   std::unique_ptr<ODESolver> RKsolver;

   inline bool print()
   {
#ifdef MFEM_USE_MPI
      return Mpi::IsInitialized() ? Mpi::Root() : true;
#else
      return true;
#endif
   }

   void CheckTimestep(real_t dt);

public:
   AdamsMoultonSolver(int s_, const real_t *a_);
   void Init(TimeDependentOperator &f_) override;
   void Step(Vector &x, real_t &t, real_t &dt) override;

   ODEStateData& GetState() override { return state; }
   const ODEStateData& GetState() const override { return state; }
};

/** A 1-stage, 2nd order AM method. */
class AM1Solver : public AdamsMoultonSolver
{
private:
   static MFEM_EXPORT const real_t a[2];

public:
   AM1Solver() : AdamsMoultonSolver(1, a) { RKsolver.reset(new SDIRK23Solver()); }
};

/** A 2-stage, 3rd order AM method. */
class AM2Solver : public AdamsMoultonSolver
{
private:
   static MFEM_EXPORT const real_t a[3];

public:
   AM2Solver() : AdamsMoultonSolver(2, a) { RKsolver.reset(new SDIRK23Solver()); }
};

/** A 3-stage, 4th order AM method. */
class AM3Solver : public AdamsMoultonSolver
{
private:
   static MFEM_EXPORT const real_t a[4];

public:
   AM3Solver() : AdamsMoultonSolver(3, a) { RKsolver.reset(new SDIRK23Solver()); }
};

/** A 4-stage, 5th order AM method. */
class AM4Solver : public AdamsMoultonSolver
{
private:
   static MFEM_EXPORT const real_t a[5];

public:
   AM4Solver() : AdamsMoultonSolver(4, a) { RKsolver.reset(new SDIRK34Solver()); }
};

/// The SIASolver class is based on the Symplectic Integration Algorithm
/// described in "A Symplectic Integration Algorithm for Separable Hamiltonian
/// Functions" by J. Candy and W. Rozmus, Journal of Computational Physics,
/// Vol. 92, pages 230-256 (1991).

/** The Symplectic Integration Algorithm (SIA) is designed for systems of first
    order ODEs derived from a Hamiltonian.
       H(q,p,t) = T(p) + V(q,t)
    Which leads to the equations:
       dq/dt = dT/dp
       dp/dt = -dV/dq
    In the integrator the operators P and F are defined to be:
       P = dT/dp
       F = -dV/dq
 */
class SIASolver
{
public:
   SIASolver() : F_(NULL), P_(NULL) {}

   virtual void Init(Operator &P, TimeDependentOperator & F);

   virtual void Step(Vector &q, Vector &p, real_t &t, real_t &dt) = 0;

   virtual void Run(Vector &q, Vector &p, real_t &t, real_t &dt, real_t tf)
   {
      while (t < tf) { Step(q, p, t, dt); }
   }

   virtual ~SIASolver() {}

protected:
   TimeDependentOperator * F_; // p_{i+1} = p_{i} + dt F(q_{i})
   Operator              * P_; // q_{i+1} = q_{i} + dt P(p_{i+1})

   mutable Vector dp_;
   mutable Vector dq_;
};

/// First Order Symplectic Integration Algorithm
class SIA1Solver : public SIASolver
{
public:
   SIA1Solver() {}
   void Step(Vector &q, Vector &p, real_t &t, real_t &dt) override;
};

/// Second Order Symplectic Integration Algorithm
class SIA2Solver : public SIASolver
{
public:
   SIA2Solver() {}
   void Step(Vector &q, Vector &p, real_t &t, real_t &dt) override;
};

/// Variable order Symplectic Integration Algorithm (orders 1-4)
class SIAVSolver : public SIASolver
{
public:
   SIAVSolver(int order);
   void Step(Vector &q, Vector &p, real_t &t, real_t &dt) override;

private:
   int order_;

   Array<real_t> a_;
   Array<real_t> b_;
};



/// Abstract class for solving systems of ODEs: d2x/dt2 = f(x,dx/dt,t)
class SecondOrderODESolver
{
protected:
   /// Pointer to the associated TimeDependentOperator.
   SecondOrderTimeDependentOperator *f;  // f(.,.,t) : R^n x R^n --> R^n
   MemoryType mem_type;
   ODEStateDataVector state;

public:
   SecondOrderODESolver() : f(NULL), state(1) { mem_type = MemoryType::HOST; }

   /// Associate a TimeDependentOperator with the ODE solver.
   /** This method has to be called:
       - Before the first call to Step().
       - When the dimensions of the associated TimeDependentOperator change.
       - When a time stepping sequence has to be restarted.
       - To change the associated TimeDependentOperator. */
   virtual void Init(SecondOrderTimeDependentOperator &f);

   /** @brief Perform a time step from time @a t [in] to time @a t [out] based
       on the requested step size @a dt [in]. */
   /** @param[in,out] x    Approximate solution.
       @param[in,out] dxdt Approximate rate.
       @param[in,out] t    Time associated with the
                           approximate solution @a x and rate @ dxdt
       @param[in,out] dt   Time step size.

       The following rules describe the common behavior of the method:
       - The input @a x [in] is the approximate solution for the input time
         @a t [in].
       - The input @a dxdt [in] is the approximate rate for the input time
         @a t [in].
       - The input @a dt [in] is the desired time step size, defining the desired
         target time: t [target] = @a t [in] + @a dt [in].
       - The output @a x [out] is the approximate solution for the output time
         @a t [out].
       - The output @a dxdt [out] is the approximate rate for the output time
         @a t [out].
       - The output @a dt [out] is the last time step taken by the method which
         may be smaller or larger than the input @a dt [in] value, e.g. because
         of time step control.
       - The method may perform more than one time step internally; in this case
         @a dt [out] is the last internal time step size.
       - The output value of @a t [out] may be smaller or larger than
         t [target], however, it is not smaller than @a t [in] + @a dt [out], if
         at least one internal time step was performed.
       - The value @a x [out] may be obtained by interpolation using internally
         stored data.
       - In some cases, the contents of @a x [in] may not be used, e.g. when
         @a x [out] from a previous Step() call was obtained by interpolation.
       - In consecutive calls to this method, the output @a t [out] of one
         Step() call has to be the same as the input @a t [in] to the next
         Step() call.
       - If the previous rule has to be broken, e.g. to restart a time stepping
         sequence, then the ODE solver must be re-initialized by calling Init()
         between the two Step() calls. */
   virtual void Step(Vector &x, Vector &dxdt, real_t &t, real_t &dt) = 0;
   void EulerStep(Vector &x, Vector &dxdt, real_t &t, real_t &dt);
   void MidPointStep(Vector &x, Vector &dxdt, real_t &t, real_t &dt);

   /// Perform time integration from time @a t [in] to time @a tf [in].
   /** @param[in,out] x    Approximate solution.
       @param[in,out] dxdt Approximate rate.
       @param[in,out] t    Time associated with the approximate solution @a x.
       @param[in,out] dt   Time step size.
       @param[in]     tf   Requested final time.

       The default implementation makes consecutive calls to Step() until
       reaching @a tf.
       The following rules describe the common behavior of the method:
       - The input @a x [in] is the approximate solution for the input time
         @a t [in].
       - The input @a dxdt [in] is the approximate rate for the input time
         @a t [in].
       - The input @a dt [in] is the initial time step size.
       - The output @a dt [out] is the last time step taken by the method which
         may be smaller or larger than the input @a dt [in] value, e.g. because
         of time step control.
       - The output value of @a t [out] is not smaller than @a tf [in]. */
   virtual void Run(Vector &x, Vector &dxdt, real_t &t, real_t &dt, real_t tf)
   {
      while (t < tf) { Step(x, dxdt, t, dt); }
   }

   /// Functions for getting the state vectors
   ODEStateData&  GetState() { return state; }
   const ODEStateData&  GetState() const { return state; }

   /// Returns how many State vectors the ODE requires
   int GetStateSize() { return GetState().MaxSize(); };

   /// Help info for SecondOrderODESolver options
   static MFEM_EXPORT std::string Types;

   /// Function selecting the desired SecondOrderODESolver
   static MFEM_EXPORT SecondOrderODESolver *Select(const int ode_solver_type);

   virtual ~SecondOrderODESolver() { }
};

/// The classical newmark method.
/// Newmark, N. M. (1959) A method of computation for structural dynamics.
/// Journal of Engineering Mechanics, ASCE, 85 (EM3) 67-94.
class NewmarkSolver : public SecondOrderODESolver
{
private:
   real_t beta, gamma;
   bool no_mult;

public:
   NewmarkSolver(real_t beta_ = 0.25, real_t gamma_ = 0.5, bool no_mult_ = false)
   {
      beta = beta_;
      gamma = gamma_;
      no_mult = no_mult_;
   };

   void PrintProperties(std::ostream &os = mfem::out);

   void Step(Vector &x, Vector &dxdt, real_t &t, real_t &dt) override;
};

class LinearAccelerationSolver : public NewmarkSolver
{
public:
   LinearAccelerationSolver() : NewmarkSolver(1.0/6.0, 0.5) { };
};

class CentralDifferenceSolver : public NewmarkSolver
{
public:
   CentralDifferenceSolver() : NewmarkSolver(0.0, 0.5) { };
};

class FoxGoodwinSolver : public NewmarkSolver
{
public:
   FoxGoodwinSolver() : NewmarkSolver(1.0/12.0, 0.5) { };
};

/// Generalized-alpha ODE solver
/// A Time Integration Algorithm for Structural Dynamics With Improved
/// Numerical Dissipation: The Generalized-α Method
/// J.Chung and G.M. Hulbert,  J. Appl. Mech 60(2), 371-375, 1993
/// https://doi.org/10.1115/1.2900803
/// rho_inf in [0,1]
class GeneralizedAlpha2Solver : public SecondOrderODESolver
{
protected:
   Vector xa,va,aa;
   real_t alpha_f, alpha_m, beta, gamma;
   bool no_mult;

public:
   GeneralizedAlpha2Solver(real_t rho_inf = 1.0, bool no_mult_ = false)
   {
      no_mult = no_mult_;
      rho_inf = (rho_inf > 1.0) ? 1.0 : rho_inf;
      rho_inf = (rho_inf < 0.0) ? 0.0 : rho_inf;

      alpha_m = (2.0 - rho_inf)/(1.0 + rho_inf);
      alpha_f = 1.0/(1.0 + rho_inf);
      beta    = 0.25*pow(1.0 + alpha_m - alpha_f,2);
      gamma   = 0.5 + alpha_m - alpha_f;
   };

   void PrintProperties(std::ostream &os = mfem::out);

   void Init(SecondOrderTimeDependentOperator &f_) override;

   void Step(Vector &x, Vector &dxdt, real_t &t, real_t &dt) override;

};

/// The classical midpoint method.
class AverageAccelerationSolver : public GeneralizedAlpha2Solver
{
public:
   AverageAccelerationSolver()
   {
      alpha_m = 0.5;
      alpha_f = 0.5;
      beta    = 0.25;
      gamma   = 0.5;
   };
};

/// HHT-alpha ODE solver
/// Improved numerical dissipation for time integration algorithms
/// in structural dynamics
/// H.M. Hilber, T.J.R. Hughes and R.L. Taylor 1977
/// https://doi.org/10.1002/eqe.4290050306
/// alpha in [2/3,1] --> Defined differently than in paper.
class HHTAlphaSolver : public GeneralizedAlpha2Solver
{
public:
   HHTAlphaSolver(real_t alpha = 1.0)
   {
      alpha = (alpha > 1.0) ? 1.0 : alpha;
      alpha = (alpha < 2.0/3.0) ? 2.0/3.0 : alpha;

      alpha_m = 1.0;
      alpha_f = alpha;
      beta    = (2-alpha)*(2-alpha)/4;
      gamma   = 0.5 + alpha_m - alpha_f;
   };

};

/// WBZ-alpha ODE solver
/// An alpha modification of Newmark's method
/// W.L. Wood, M. Bossak and O.C. Zienkiewicz 1980
/// https://doi.org/10.1002/nme.1620151011
/// rho_inf in [0,1]
class WBZAlphaSolver : public GeneralizedAlpha2Solver
{
public:
   WBZAlphaSolver(real_t rho_inf = 1.0)
   {
      rho_inf = (rho_inf > 1.0) ? 1.0 : rho_inf;
      rho_inf = (rho_inf < 0.0) ? 0.0 : rho_inf;

      alpha_f = 1.0;
      alpha_m = 2.0/(1.0 + rho_inf);
      beta    = 0.25*pow(1.0 + alpha_m - alpha_f,2);
      gamma   = 0.5 + alpha_m - alpha_f;
   };

};

}

#endif
