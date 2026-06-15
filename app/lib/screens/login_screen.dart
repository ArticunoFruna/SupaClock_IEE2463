import 'package:flutter/material.dart';
import '../services/auth_service.dart';
import '../services/firestore_service.dart';
import '../config/theme.dart';
import 'dashboard_screen.dart';

class LoginScreen extends StatefulWidget {
  const LoginScreen({super.key});

  @override
  State<LoginScreen> createState() => _LoginScreenState();
}

class _LoginScreenState extends State<LoginScreen>
    with SingleTickerProviderStateMixin {
  final _authService = AuthService();
  final _firestoreService = FirestoreService();
  final _emailController = TextEditingController();
  final _passwordController = TextEditingController();
  final _nameController = TextEditingController();
  final _formKey = GlobalKey<FormState>();

  bool _isLogin = true;
  bool _isLoading = false;
  String? _error;

  late AnimationController _animController;
  late Animation<double> _fadeAnim;

  @override
  void initState() {
    super.initState();
    _animController = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 800),
    );
    _fadeAnim = CurvedAnimation(
      parent: _animController,
      curve: Curves.easeOut,
    );
    _animController.forward();
  }

  @override
  void dispose() {
    _animController.dispose();
    _emailController.dispose();
    _passwordController.dispose();
    _nameController.dispose();
    super.dispose();
  }

  Future<void> _handleEmailAuth() async {
    if (!_formKey.currentState!.validate()) return;

    setState(() {
      _isLoading = true;
      _error = null;
    });

    try {
      if (_isLogin) {
        await _authService.signInWithEmail(
          email: _emailController.text.trim(),
          password: _passwordController.text,
        );
      } else {
        final credential = await _authService.registerWithEmail(
          email: _emailController.text.trim(),
          password: _passwordController.text,
        );
        // Create user profile in Firestore
        if (credential.user != null) {
          await _firestoreService.getOrCreateUser(
            credential.user!.uid,
            name: _nameController.text.trim(),
          );
          await _authService.updateDisplayName(_nameController.text.trim());
        }
      }
      if (mounted) _navigateToDashboard();
    } catch (e) {
      setState(() => _error = _parseAuthError(e.toString()));
    } finally {
      if (mounted) setState(() => _isLoading = false);
    }
  }

  Future<void> _handleGoogleSignIn() async {
    setState(() {
      _isLoading = true;
      _error = null;
    });

    try {
      final credential = await _authService.signInWithGoogle();
      if (credential?.user != null) {
        await _firestoreService.getOrCreateUser(
          credential!.user!.uid,
          name: credential.user!.displayName,
        );
        if (mounted) _navigateToDashboard();
      }
    } catch (e) {
      setState(() => _error = _parseAuthError(e.toString()));
    } finally {
      if (mounted) setState(() => _isLoading = false);
    }
  }

  void _navigateToDashboard() {
    Navigator.of(context).pushReplacement(
      MaterialPageRoute(builder: (_) => const DashboardScreen()),
    );
  }

  String _parseAuthError(String error) {
    if (error.contains('user-not-found')) return 'Usuario no encontrado';
    if (error.contains('wrong-password')) return 'Contraseña incorrecta';
    if (error.contains('email-already-in-use')) return 'El email ya está registrado';
    if (error.contains('weak-password')) return 'La contraseña es muy débil';
    if (error.contains('invalid-email')) return 'Email inválido';
    if (error.contains('network')) return 'Error de conexión';
    return 'Error de autenticación. Intenta de nuevo.';
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: SafeArea(
        child: Center(
          child: SingleChildScrollView(
            padding: const EdgeInsets.symmetric(horizontal: 32),
            child: FadeTransition(
              opacity: _fadeAnim,
              child: ConstrainedBox(
                constraints: const BoxConstraints(maxWidth: 420),
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    // Logo & Title
                    _buildHeader(),
                    const SizedBox(height: 48),

                    // Auth Form
                    _buildForm(),
                    const SizedBox(height: 16),

                    // Error message
                    if (_error != null) _buildError(),

                    const SizedBox(height: 24),

                    // Submit button
                    _buildSubmitButton(),
                    const SizedBox(height: 16),

                    // Divider
                    _buildDivider(),
                    const SizedBox(height: 16),

                    // Google Sign In
                    _buildGoogleButton(),
                    const SizedBox(height: 24),

                    // Toggle login/register
                    _buildToggle(),
                  ],
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildHeader() {
    return Column(
      children: [
        // Animated pulse icon
        Container(
          width: 80,
          height: 80,
          decoration: BoxDecoration(
            shape: BoxShape.circle,
            gradient: LinearGradient(
              colors: [
                AppTheme.primary.withValues(alpha: 0.8),
                AppTheme.secondary.withValues(alpha: 0.6),
              ],
              begin: Alignment.topLeft,
              end: Alignment.bottomRight,
            ),
            boxShadow: [
              BoxShadow(
                color: AppTheme.primary.withValues(alpha: 0.3),
                blurRadius: 24,
                spreadRadius: 4,
              ),
            ],
          ),
          child: const Icon(
            Icons.watch,
            size: 40,
            color: Colors.white,
          ),
        ),
        const SizedBox(height: 24),
        Text(
          'SupaClock',
          style: Theme.of(context).textTheme.displayLarge?.copyWith(
            foreground: Paint()
              ..shader = const LinearGradient(
                colors: [AppTheme.primary, AppTheme.secondary],
              ).createShader(const Rect.fromLTWH(0, 0, 200, 40)),
          ),
        ),
        const SizedBox(height: 8),
        Text(
          'Wearable Biométrico Modular',
          style: Theme.of(context).textTheme.bodyMedium,
        ),
      ],
    );
  }

  Widget _buildForm() {
    return Form(
      key: _formKey,
      child: Column(
        children: [
          // Name field (only for register)
          if (!_isLogin) ...[
            TextFormField(
              controller: _nameController,
              decoration: const InputDecoration(
                hintText: 'Nombre',
                prefixIcon: Icon(Icons.person_outline),
              ),
              validator: (v) =>
                  v == null || v.isEmpty ? 'Ingresa tu nombre' : null,
            ),
            const SizedBox(height: 12),
          ],

          // Email
          TextFormField(
            controller: _emailController,
            keyboardType: TextInputType.emailAddress,
            decoration: const InputDecoration(
              hintText: 'Email',
              prefixIcon: Icon(Icons.email_outlined),
            ),
            validator: (v) =>
                v == null || !v.contains('@') ? 'Email inválido' : null,
          ),
          const SizedBox(height: 12),

          // Password
          TextFormField(
            controller: _passwordController,
            obscureText: true,
            decoration: const InputDecoration(
              hintText: 'Contraseña',
              prefixIcon: Icon(Icons.lock_outline),
            ),
            validator: (v) => v == null || v.length < 6
                ? 'Mínimo 6 caracteres'
                : null,
          ),
        ],
      ),
    );
  }

  Widget _buildError() {
    return Container(
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: AppTheme.danger.withValues(alpha: 0.1),
        borderRadius: BorderRadius.circular(8),
        border: Border.all(color: AppTheme.danger.withValues(alpha: 0.3)),
      ),
      child: Row(
        children: [
          const Icon(Icons.error_outline, color: AppTheme.danger, size: 20),
          const SizedBox(width: 8),
          Expanded(
            child: Text(
              _error!,
              style: const TextStyle(color: AppTheme.danger, fontSize: 13),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildSubmitButton() {
    return SizedBox(
      width: double.infinity,
      child: ElevatedButton(
        onPressed: _isLoading ? null : _handleEmailAuth,
        child: _isLoading
            ? const SizedBox(
                width: 20,
                height: 20,
                child: CircularProgressIndicator(
                  strokeWidth: 2,
                  color: Colors.white,
                ),
              )
            : Text(_isLogin ? 'Iniciar Sesión' : 'Crear Cuenta'),
      ),
    );
  }

  Widget _buildDivider() {
    return Row(
      children: [
        Expanded(child: Divider(color: AppTheme.borderColor)),
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 16),
          child: Text('o', style: TextStyle(color: AppTheme.textMuted)),
        ),
        Expanded(child: Divider(color: AppTheme.borderColor)),
      ],
    );
  }

  Widget _buildGoogleButton() {
    return SizedBox(
      width: double.infinity,
      child: OutlinedButton.icon(
        onPressed: _isLoading ? null : _handleGoogleSignIn,
        icon: const Icon(Icons.g_mobiledata, size: 24),
        label: const Text('Continuar con Google'),
      ),
    );
  }

  Widget _buildToggle() {
    return TextButton(
      onPressed: () {
        setState(() {
          _isLogin = !_isLogin;
          _error = null;
        });
      },
      child: Text(
        _isLogin
            ? '¿No tienes cuenta? Regístrate'
            : '¿Ya tienes cuenta? Inicia sesión',
        style: const TextStyle(color: AppTheme.primary),
      ),
    );
  }
}
